/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/Assertions.h"

#include "jsapi.h"
#include "xpcpublic.h"
#include "nsIGlobalObject.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsPIDOMWindow.h"
#include "nsTArray.h"
#include "nsJSUtils.h"

namespace mozilla {
namespace dom {

static mozilla::ThreadLocal<ScriptSettingsStackEntry*> sScriptSettingsTLS;

class ScriptSettingsStack {
public:
  static ScriptSettingsStackEntry* Top() {
    return sScriptSettingsTLS.get();
  }

  static void Push(ScriptSettingsStackEntry *aEntry) {
    MOZ_ASSERT(!aEntry->mOlder);
    // Whenever JSAPI use is disabled, the next stack entry pushed must
    // always be a candidate entry point.
    MOZ_ASSERT_IF(!Top() || Top()->NoJSAPI(), aEntry->mIsCandidateEntryPoint);

    aEntry->mOlder = Top();
    sScriptSettingsTLS.set(aEntry);
  }

  static void Pop(ScriptSettingsStackEntry *aEntry) {
    MOZ_ASSERT(aEntry == Top());
    sScriptSettingsTLS.set(aEntry->mOlder);
  }

  static nsIGlobalObject* IncumbentGlobal() {
    ScriptSettingsStackEntry *entry = Top();
    return entry ? entry->mGlobalObject : nullptr;
  }

  static ScriptSettingsStackEntry* EntryPoint() {
    ScriptSettingsStackEntry *entry = Top();
    if (!entry) {
      return nullptr;
    }
    while (entry) {
      if (entry->mIsCandidateEntryPoint)
        return entry;
      entry = entry->mOlder;
    }
    MOZ_CRASH("Non-empty stack should always have an entry point");
  }

  static nsIGlobalObject* EntryGlobal() {
    ScriptSettingsStackEntry *entry = EntryPoint();
    return entry ? entry->mGlobalObject : nullptr;
  }
};

void
InitScriptSettings()
{
  if (!sScriptSettingsTLS.initialized()) {
    bool success = sScriptSettingsTLS.init();
    if (!success) {
      MOZ_CRASH();
    }
  }

  sScriptSettingsTLS.set(nullptr);
}

void DestroyScriptSettings()
{
  MOZ_ASSERT(sScriptSettingsTLS.get() == nullptr);
}

ScriptSettingsStackEntry::ScriptSettingsStackEntry(nsIGlobalObject *aGlobal,
                                                   bool aCandidate)
  : mGlobalObject(aGlobal)
  , mIsCandidateEntryPoint(aCandidate)
  , mOlder(nullptr)
{
  MOZ_ASSERT(mGlobalObject);
  MOZ_ASSERT(mGlobalObject->GetGlobalJSObject(),
             "Must have an actual JS global for the duration on the stack");
  MOZ_ASSERT(JS_IsGlobalObject(mGlobalObject->GetGlobalJSObject()),
             "No outer windows allowed");

  ScriptSettingsStack::Push(this);
}

// This constructor is only for use by AutoNoJSAPI.
ScriptSettingsStackEntry::ScriptSettingsStackEntry()
   : mGlobalObject(nullptr)
   , mIsCandidateEntryPoint(true)
   , mOlder(nullptr)
{
  ScriptSettingsStack::Push(this);
}

ScriptSettingsStackEntry::~ScriptSettingsStackEntry()
{
  // We must have an actual JS global for the entire time this is on the stack.
  MOZ_ASSERT_IF(mGlobalObject, mGlobalObject->GetGlobalJSObject());

  ScriptSettingsStack::Pop(this);
}

// This mostly gets the entry global, but doesn't entirely match the spec in
// certain edge cases. It's good enough for some purposes, but not others. If
// you want to call this function, ping bholley and describe your use-case.
nsIGlobalObject*
BrokenGetEntryGlobal()
{
  // We need the current JSContext in order to check the JS for
  // scripted frames that may have appeared since anyone last
  // manipulated the stack. If it's null, that means that there
  // must be no entry global on the stack.
  JSContext *cx = nsContentUtils::GetCurrentJSContextForThread();
  if (!cx) {
    MOZ_ASSERT(ScriptSettingsStack::EntryGlobal() == nullptr);
    return nullptr;
  }

  return nsJSUtils::GetDynamicScriptGlobal(cx);
}

// Note: When we're ready to expose it, GetEntryGlobal will look similar to
// GetIncumbentGlobal below.

nsIGlobalObject*
GetIncumbentGlobal()
{
  // We need the current JSContext in order to check the JS for
  // scripted frames that may have appeared since anyone last
  // manipulated the stack. If it's null, that means that there
  // must be no entry global on the stack, and therefore no incumbent
  // global either.
  JSContext *cx = nsContentUtils::GetCurrentJSContextForThread();
  if (!cx) {
    MOZ_ASSERT(ScriptSettingsStack::EntryGlobal() == nullptr);
    return nullptr;
  }

  // See what the JS engine has to say. If we've got a scripted caller
  // override in place, the JS engine will lie to us and pretend that
  // there's nothing on the JS stack, which will cause us to check the
  // incumbent script stack below.
  if (JSObject *global = JS::GetScriptedCallerGlobal(cx)) {
    return xpc::GetNativeForGlobal(global);
  }

  // Ok, nothing from the JS engine. Let's use whatever's on the
  // explicit stack.
  return ScriptSettingsStack::IncumbentGlobal();
}

nsIPrincipal*
GetWebIDLCallerPrincipal()
{
  MOZ_ASSERT(NS_IsMainThread());
  ScriptSettingsStackEntry *entry = ScriptSettingsStack::EntryPoint();

  // If we have an entry point that is not NoJSAPI, we know it must be an
  // AutoEntryScript.
  if (!entry || entry->NoJSAPI()) {
    return nullptr;
  }
  AutoEntryScript* aes = static_cast<AutoEntryScript*>(entry);

  // We can't yet rely on the Script Settings Stack to properly determine the
  // entry script, because there are still lots of places in the tree where we
  // don't yet use an AutoEntryScript (bug 951991 tracks this work). In the
  // mean time though, we can make some observations to hack around the
  // problem:
  //
  // (1) All calls into JS-implemented WebIDL go through CallSetup, which goes
  //     through AutoEntryScript.
  // (2) The top candidate entry point in the Script Settings Stack is the
  //     entry point if and only if no other JSContexts have been pushed on
  //     top of the push made by that entry's AutoEntryScript.
  //
  // Because of (1), all of the cases where we might return a non-null
  // WebIDL Caller are guaranteed to have put an entry on the Script Settings
  // Stack, so we can restrict our search to that. Moreover, (2) gives us a
  // criterion to determine whether an entry in the Script Setting Stack means
  // that we should return a non-null WebIDL Caller.
  //
  // Once we fix bug 951991, this can all be simplified.
  if (!aes->CxPusherIsStackTop()) {
    return nullptr;
  }

  return aes->mWebIDLCallerPrincipal;
}

static JSContext*
FindJSContext(nsIGlobalObject* aGlobalObject)
{
  MOZ_ASSERT(NS_IsMainThread());
  JSContext *cx = nullptr;
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aGlobalObject);
  if (sgo && sgo->GetScriptContext()) {
    cx = sgo->GetScriptContext()->GetNativeContext();
  }
  if (!cx) {
    cx = nsContentUtils::GetSafeJSContext();
  }
  return cx;
}

AutoJSAPI::AutoJSAPI()
  : mCx(nullptr)
{
}

void
AutoJSAPI::InitInternal(JSObject* aGlobal, JSContext* aCx, bool aIsMainThread)
{
  mCx = aCx;
  if (aIsMainThread) {
    mCxPusher.construct(mCx);
  }

  mAutoNullableCompartment.construct(mCx, aGlobal);
}

AutoJSAPI::AutoJSAPI(nsIGlobalObject* aGlobalObject,
                     bool aIsMainThread,
                     JSContext* aCx)
{
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT(aGlobalObject->GetGlobalJSObject(), "Must have a JS global");
  MOZ_ASSERT(aCx);
  MOZ_ASSERT_IF(aIsMainThread, NS_IsMainThread());

  InitInternal(aGlobalObject->GetGlobalJSObject(), aCx, aIsMainThread);
}

void
AutoJSAPI::Init()
{
  MOZ_ASSERT(!mCx, "An AutoJSAPI should only be initialised once");

  InitInternal(/* aGlobal */ nullptr,
               nsContentUtils::GetDefaultJSContextForThread(),
               NS_IsMainThread());
}

bool
AutoJSAPI::Init(nsIGlobalObject* aGlobalObject, JSContext* aCx)
{
  MOZ_ASSERT(!mCx, "An AutoJSAPI should only be initialised once");
  MOZ_ASSERT(aCx);

  if (NS_WARN_IF(!aGlobalObject)) {
    return false;
  }

  JSObject* global = aGlobalObject->GetGlobalJSObject();
  if (NS_WARN_IF(!global)) {
    return false;
  }

  InitInternal(global, aCx, NS_IsMainThread());
  return true;
}

bool
AutoJSAPI::Init(nsIGlobalObject* aGlobalObject)
{
  return Init(aGlobalObject, nsContentUtils::GetDefaultJSContextForThread());
}

bool
AutoJSAPI::InitWithLegacyErrorReporting(nsIGlobalObject* aGlobalObject)
{
  MOZ_ASSERT(NS_IsMainThread());

  return Init(aGlobalObject, FindJSContext(aGlobalObject));
}

bool
AutoJSAPI::InitUsingWin(nsPIDOMWindow* aWindow, JSContext* aCx)
{
  return Init(static_cast<nsGlobalWindow*>(aWindow), aCx);
}

bool
AutoJSAPI::InitUsingWin(nsPIDOMWindow* aWindow)
{
  return Init(static_cast<nsGlobalWindow*>(aWindow));
}

bool
AutoJSAPI::InitWithLegacyErrorReportingUsingWin(nsPIDOMWindow* aWindow)
{
  return InitWithLegacyErrorReporting(static_cast<nsGlobalWindow*>(aWindow));
}

AutoEntryScript::AutoEntryScript(nsIGlobalObject* aGlobalObject,
                                 bool aIsMainThread,
                                 JSContext* aCx)
  : AutoJSAPI(aGlobalObject, aIsMainThread,
              aCx ? aCx : FindJSContext(aGlobalObject))
  , ScriptSettingsStackEntry(aGlobalObject, /* aCandidate = */ true)
  , mWebIDLCallerPrincipal(nullptr)
{
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT_IF(!aCx, aIsMainThread); // cx is mandatory off-main-thread.
  MOZ_ASSERT_IF(aCx && aIsMainThread, aCx == FindJSContext(aGlobalObject));
}

AutoIncumbentScript::AutoIncumbentScript(nsIGlobalObject* aGlobalObject)
  : ScriptSettingsStackEntry(aGlobalObject, /* aCandidate = */ false)
  , mCallerOverride(nsContentUtils::GetCurrentJSContextForThread())
{
}

AutoNoJSAPI::AutoNoJSAPI(bool aIsMainThread)
  : ScriptSettingsStackEntry()
{
  MOZ_ASSERT_IF(nsContentUtils::GetCurrentJSContextForThread(),
                !JS_IsExceptionPending(nsContentUtils::GetCurrentJSContextForThread()));
  if (aIsMainThread) {
    mCxPusher.construct(static_cast<JSContext*>(nullptr),
                        /* aAllowNull = */ true);
  }
}

} // namespace dom
} // namespace mozilla
