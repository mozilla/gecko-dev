/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JSControl.h"

#include "mozilla/Base64.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/JSON.h"
#include "js/PropertySpec.h"
#include "nsImportModule.h"
#include "rrIModule.h"
#include "xpcprivate.h"
#include "nsMediaFeatures.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace JS;

namespace mozilla {
namespace recordreplay {
namespace js {

// Callback for filling CharBuffers when converting objects to JSON.
static bool FillCharBufferCallback(const char16_t* buf, uint32_t len,
                                   void* data) {
  CharBuffer* buffer = (CharBuffer*)data;
  MOZ_RELEASE_ASSERT(buffer->length() == 0);
  buffer->append(buf, len);
  return true;
}

static JSObject* RequireObject(JSContext* aCx, HandleValue aValue) {
  if (!aValue.isObject()) {
    JS_ReportErrorASCII(aCx, "Expected object");
    return nullptr;
  }
  return &aValue.toObject();
}

static bool RequireNumber(JSContext* aCx, HandleValue aValue, size_t* aNumber) {
  if (!aValue.isNumber()) {
    JS_ReportErrorASCII(aCx, "Expected number");
    return false;
  }
  *aNumber = aValue.toNumber();
  return true;
}

static void InitializeScriptHits();

static nsCString gModuleText;

void ReadReplayJS(const char* aFile) {
  int fd = open(aFile, O_RDONLY);
  MOZ_RELEASE_ASSERT(fd >= 0);

  struct stat info;
  fstat(fd, &info);

  gModuleText.SetLength(info.st_size);
  int nread = read(fd, gModuleText.BeginWriting(), info.st_size);
  MOZ_RELEASE_ASSERT(nread == info.st_size);

  close(fd);
}

// URL of the root module script.
#define ModuleURL "resource://devtools/server/actors/replay/module.js"

static StaticRefPtr<rrIModule> gModule;
static PersistentRootedObject* gModuleObject;

bool IsInitialized() {
  return !!gModule;
}

static void EnsureInitialized() {
  if (IsInitialized()) {
    return;
  }

  // Initialization so we can repaint at the first checkpoint without having
  // an unhandled recording divergence.
  nsMediaFeatures::InitSystemMetrics();

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  nsCOMPtr<rrIModule> module = do_ImportModule(ModuleURL);
  gModule = module.forget();
  ClearOnShutdown(&gModule);

  if (IsReplaying()) {
    MOZ_RELEASE_ASSERT(gModuleText.Length());
  }

  RootedValue value(cx);
  if (NS_FAILED(gModule->Initialize(gModuleText, &value))) {
    MOZ_CRASH("EnsureInitialized: Initialize failed");
  }
  MOZ_RELEASE_ASSERT(value.isObject());

  gModuleObject = new PersistentRootedObject(cx);
  *gModuleObject = &value.toObject();

  if (IsRecordingOrReplaying()) {
    InitializeScriptHits();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Devtools Sandbox
///////////////////////////////////////////////////////////////////////////////

extern "C" {

MOZ_EXPORT bool RecordReplayInterface_ShouldUpdateProgressCounter(
    const char* aURL) {
  // Progress counters are only updated for scripts which are exposed to the
  // debugger. The devtools timeline is based on progress values and we don't
  // want gaps on the timeline which users can't seek to.
  return aURL && strncmp(aURL, "resource:", 9) && strncmp(aURL, "chrome:", 7);
}

}  // extern "C"

extern "C" {

MOZ_EXPORT ProgressCounter* RecordReplayInterface_ExecutionProgressCounter() {
  MOZ_CRASH("FIXME");
  return nullptr;
}

MOZ_EXPORT void RecordReplayInterface_AdvanceExecutionProgressCounter() {
}

MOZ_EXPORT ProgressCounter RecordReplayInterface_NewTimeWarpTarget() {
  if (AreThreadEventsDisallowed()) {
    return 0;
  }

  // NewTimeWarpTarget() must be called at consistent points between recording
  // and replaying.
  RecordReplayAssert("NewTimeWarpTarget");

  if (!IsInitialized() || IsRecording()) {
    return 0;
  }

  AutoDisallowThreadEvents disallow;
  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  RootedValue rv(cx);
  if (!JS_CallFunctionName(cx, *gModuleObject, "NewTimeWarpTarget", HandleValueArray::empty(), &rv)) {
    MOZ_CRASH("NewTimeWarpTarget");
  }

  MOZ_RELEASE_ASSERT(rv.isNumber());
  return rv.toNumber();
}

}  // extern "C"

void OnTestCommand(const char* aString) {
  // Ignore commands to finish the current test if we aren't recording/replaying.
  if (!strcmp(aString, "RecReplaySendAsyncMessage RecordingFinished") &&
      !IsRecordingOrReplaying()) {
    return;
  }

  EnsureInitialized();

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  JSString* str = JS_NewStringCopyZ(cx, aString);
  MOZ_RELEASE_ASSERT(str);

  JS::AutoValueArray<1> args(cx);
  args[0].setString(str);

  RootedValue rv(cx);
  if (!JS_CallFunctionName(cx, *gModuleObject, "OnTestCommand", args, &rv)) {
    MOZ_CRASH("OnTestCommand");
  }
}

extern "C" {

MOZ_EXPORT void RecordReplayInterface_BeginContentParse(
    const void* aToken, const char* aURL, const char* aContentType) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(aToken);
}

MOZ_EXPORT void RecordReplayInterface_AddContentParseData8(
    const void* aToken, const Utf8Unit* aUtf8Buffer, size_t aLength) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(aToken);
}

MOZ_EXPORT void RecordReplayInterface_AddContentParseData16(
    const void* aToken, const char16_t* aBuffer, size_t aLength) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(aToken);
}

MOZ_EXPORT void RecordReplayInterface_EndContentParse(const void* aToken) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(aToken);
}

}  // extern "C"

///////////////////////////////////////////////////////////////////////////////
// Plumbing
///////////////////////////////////////////////////////////////////////////////

static const JSFunctionSpec gRecordReplayMethods[] = {
    JS_FS_END};

extern "C" {

MOZ_EXPORT bool RecordReplayInterface_DefineRecordReplayControlObject(
    void* aCxVoid, void* aObjectArg) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());

  JSContext* aCx = static_cast<JSContext*>(aCxVoid);
  RootedObject object(aCx, static_cast<JSObject*>(aObjectArg));

  RootedObject staticObject(aCx, JS_NewObject(aCx, nullptr));
  if (!staticObject ||
      !JS_DefineProperty(aCx, object, "RecordReplayControl", staticObject, 0)) {
    return false;
  }

  if (gModuleObject) {
    // RecordReplayControl objects created while setting up the module itself
    // don't get references to the module.
    RootedObject obj(aCx, *gModuleObject);
    if (!JS_WrapObject(aCx, &obj) ||
        !JS_DefineProperty(aCx, staticObject, "module", obj, 0)) {
      return false;
    }
  }

  if (!JS_DefineFunctions(aCx, staticObject, gRecordReplayMethods)) {
    return false;
  }

  return true;
}

}  // extern "C"

}  // namespace js
}  // namespace recordreplay
}  // namespace mozilla
