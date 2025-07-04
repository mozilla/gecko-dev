/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoadedScript.h"

#include "mozilla/AlreadyAddRefed.h"  // already_AddRefed
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/RefPtr.h"     // RefPtr, mozilla::MakeRefPtr
#include "mozilla/UniquePtr.h"  // mozilla::UniquePtr

#include "mozilla/dom/ScriptLoadContext.h"  // ScriptLoadContext
#include "jsfriendapi.h"
#include "js/Modules.h"       // JS::{Get,Set}ModulePrivate
#include "LoadContextBase.h"  // LoadContextBase

namespace JS::loader {

//////////////////////////////////////////////////////////////
// LoadedScript
//////////////////////////////////////////////////////////////

MOZ_DEFINE_MALLOC_SIZE_OF(LoadedScriptMallocSizeOf)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LoadedScript)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(LoadedScript, mFetchOptions, mURI, mBaseURL)

NS_IMPL_CYCLE_COLLECTING_ADDREF(LoadedScript)
NS_IMPL_CYCLE_COLLECTING_RELEASE(LoadedScript)

LoadedScript::LoadedScript(ScriptKind aKind,
                           mozilla::dom::ReferrerPolicy aReferrerPolicy,
                           ScriptFetchOptions* aFetchOptions, nsIURI* aURI)
    : mKind(aKind),
      mReferrerPolicy(aReferrerPolicy),
      mFetchOptions(aFetchOptions),
      mURI(aURI),
      mDataType(DataType::eUnknown),
      mReceivedScriptTextLength(0),
      mBytecodeOffset(0) {
  MOZ_ASSERT(mFetchOptions);
  MOZ_ASSERT(mURI);
}

LoadedScript::LoadedScript(const LoadedScript& aOther)
    : mKind(aOther.mKind),
      mReferrerPolicy(aOther.mReferrerPolicy),
      mFetchOptions(aOther.mFetchOptions),
      mURI(aOther.mURI),
      mBaseURL(aOther.mBaseURL),
      mDataType(DataType::eStencil),
      mReceivedScriptTextLength(0),
      mBytecodeOffset(0),
      mStencil(aOther.mStencil) {
  MOZ_ASSERT(mFetchOptions);
  MOZ_ASSERT(mURI);
  // NOTE: This is only for the stencil case.
  //       The script text and the bytecode are not reflected.
  MOZ_DIAGNOSTIC_ASSERT(aOther.mDataType == DataType::eStencil);
  MOZ_DIAGNOSTIC_ASSERT(mStencil);
  MOZ_ASSERT(!mScriptData);
  MOZ_ASSERT(mScriptBytecode.empty());
}

LoadedScript::~LoadedScript() {
  mozilla::UnregisterWeakMemoryReporter(this);
  mozilla::DropJSObjects(this);
}

void LoadedScript::RegisterMemoryReport() {
  mozilla::RegisterWeakMemoryReporter(this);
}

NS_IMETHODIMP
LoadedScript::CollectReports(nsIHandleReportCallback* aHandleReport,
                             nsISupports* aData, bool aAnonymize) {
#define COLLECT_REPORT(path, kind)                                   \
  MOZ_COLLECT_REPORT(path, KIND_HEAP, UNITS_BYTES,                   \
                     SizeOfIncludingThis(LoadedScriptMallocSizeOf),  \
                     "Memory used for LoadedScript to hold on " kind \
                     " across documents")

  switch (mKind) {
    case ScriptKind::eClassic:
      COLLECT_REPORT("explicit/js/script/loaded-script/classic", "scripts");
      break;
    case ScriptKind::eImportMap:
      COLLECT_REPORT("explicit/js/script/loaded-script/import-map",
                     "import-maps");
      break;
    case ScriptKind::eModule:
      COLLECT_REPORT("explicit/js/script/loaded-script/module", "modules");
      break;
    case ScriptKind::eEvent:
      COLLECT_REPORT("explicit/js/script/loaded-script/event", "event scripts");
      break;
  }

#undef COLLECT_REPORT
  return NS_OK;
}

size_t LoadedScript::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t bytes = aMallocSizeOf(this);

  if (IsTextSource()) {
    if (IsUTF16Text()) {
      bytes += ScriptText<char16_t>().sizeOfExcludingThis(aMallocSizeOf);
    } else {
      bytes += ScriptText<Utf8Unit>().sizeOfExcludingThis(aMallocSizeOf);
    }
  }

  bytes += mScriptBytecode.sizeOfExcludingThis(aMallocSizeOf);

  // NOTE: Stencil is reported by SpiderMonkey.
  return bytes;
}

void LoadedScript::AssociateWithScript(JSScript* aScript) {
  // Verify that the rewritten URL is available when manipulating LoadedScript.
  MOZ_ASSERT(mBaseURL);

  // Set a JSScript's private value to point to this object. The JS engine will
  // increment our reference count by calling HostAddRefTopLevelScript(). This
  // is decremented by HostReleaseTopLevelScript() below when the JSScript dies.

  MOZ_ASSERT(JS::GetScriptPrivate(aScript).isUndefined());
  JS::SetScriptPrivate(aScript, JS::PrivateValue(this));
}

nsresult LoadedScript::GetScriptSource(JSContext* aCx,
                                       MaybeSourceText* aMaybeSource,
                                       LoadContextBase* aMaybeLoadContext) {
  // If there's no script text, we try to get it from the element
  bool isWindowContext =
      aMaybeLoadContext && aMaybeLoadContext->IsWindowContext();
  if (isWindowContext && aMaybeLoadContext->AsWindowContext()->mIsInline) {
    nsAutoString inlineData;
    auto* scriptLoadContext = aMaybeLoadContext->AsWindowContext();
    scriptLoadContext->GetInlineScriptText(inlineData);

    size_t nbytes = inlineData.Length() * sizeof(char16_t);
    JS::UniqueTwoByteChars chars(
        static_cast<char16_t*>(JS_malloc(aCx, nbytes)));
    if (!chars) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    memcpy(chars.get(), inlineData.get(), nbytes);

    SourceText<char16_t> srcBuf;
    if (!srcBuf.init(aCx, std::move(chars), inlineData.Length())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    aMaybeSource->construct<SourceText<char16_t>>(std::move(srcBuf));
    return NS_OK;
  }

  size_t length = ScriptTextLength();
  if (IsUTF16Text()) {
    JS::UniqueTwoByteChars chars;
    chars.reset(ScriptText<char16_t>().extractOrCopyRawBuffer());
    if (!chars) {
      JS_ReportOutOfMemory(aCx);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    SourceText<char16_t> srcBuf;
    if (!srcBuf.init(aCx, std::move(chars), length)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    aMaybeSource->construct<SourceText<char16_t>>(std::move(srcBuf));
    return NS_OK;
  }

  MOZ_ASSERT(IsUTF8Text());
  mozilla::UniquePtr<Utf8Unit[], JS::FreePolicy> chars;
  chars.reset(ScriptText<Utf8Unit>().extractOrCopyRawBuffer());
  if (!chars) {
    JS_ReportOutOfMemory(aCx);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(aCx, std::move(chars), length)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  aMaybeSource->construct<SourceText<Utf8Unit>>(std::move(srcBuf));
  return NS_OK;
}

inline void CheckModuleScriptPrivate(LoadedScript* script,
                                     const JS::Value& aPrivate) {
#ifdef DEBUG
  if (script->IsModuleScript()) {
    JSObject* module = script->AsModuleScript()->mModuleRecord.unbarrieredGet();
    MOZ_ASSERT_IF(module, JS::GetModulePrivate(module) == aPrivate);
  }
#endif
}

void HostAddRefTopLevelScript(const JS::Value& aPrivate) {
  // Increment the reference count of a LoadedScript object that is now pointed
  // to by a JSScript. The reference count is decremented by
  // HostReleaseTopLevelScript() below.

  auto script = static_cast<LoadedScript*>(aPrivate.toPrivate());
  CheckModuleScriptPrivate(script, aPrivate);
  script->AddRef();
}

void HostReleaseTopLevelScript(const JS::Value& aPrivate) {
  // Decrement the reference count of a LoadedScript object that was pointed to
  // by a JSScript. The reference count was originally incremented by
  // HostAddRefTopLevelScript() above.

  auto script = static_cast<LoadedScript*>(aPrivate.toPrivate());
  CheckModuleScriptPrivate(script, aPrivate);
  script->Release();
}

//////////////////////////////////////////////////////////////
// EventScript
//////////////////////////////////////////////////////////////

EventScript::EventScript(mozilla::dom::ReferrerPolicy aReferrerPolicy,
                         ScriptFetchOptions* aFetchOptions, nsIURI* aURI)
    : LoadedScript(ScriptKind::eEvent, aReferrerPolicy, aFetchOptions, aURI) {
  // EventScripts are not using ScriptLoadRequest, and mBaseURL and mURI are
  // the same thing.
  SetBaseURL(aURI);
}

//////////////////////////////////////////////////////////////
// ClassicScript
//////////////////////////////////////////////////////////////

ClassicScript::ClassicScript(mozilla::dom::ReferrerPolicy aReferrerPolicy,
                             ScriptFetchOptions* aFetchOptions, nsIURI* aURI)
    : LoadedScript(ScriptKind::eClassic, aReferrerPolicy, aFetchOptions, aURI) {
}

//////////////////////////////////////////////////////////////
// ModuleScript
//////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(ModuleScript, LoadedScript)

NS_IMPL_CYCLE_COLLECTION_CLASS(ModuleScript)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ModuleScript, LoadedScript)
  tmp->UnlinkModuleRecord();
  tmp->mParseError.setUndefined();
  tmp->mErrorToRethrow.setUndefined();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ModuleScript, LoadedScript)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ModuleScript, LoadedScript)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mModuleRecord)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mParseError)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mErrorToRethrow)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

ModuleScript::ModuleScript(mozilla::dom::ReferrerPolicy aReferrerPolicy,
                           ScriptFetchOptions* aFetchOptions, nsIURI* aURI)
    : LoadedScript(ScriptKind::eModule, aReferrerPolicy, aFetchOptions, aURI) {
  MOZ_ASSERT(!ModuleRecord());
  MOZ_ASSERT(!HasParseError());
  MOZ_ASSERT(!HasErrorToRethrow());
}

ModuleScript::ModuleScript(const LoadedScript& aOther) : LoadedScript(aOther) {
  MOZ_ASSERT(!ModuleRecord());
  MOZ_ASSERT(!HasParseError());
  MOZ_ASSERT(!HasErrorToRethrow());
}

/* static */
already_AddRefed<ModuleScript> ModuleScript::FromCache(
    const LoadedScript& aScript) {
  MOZ_DIAGNOSTIC_ASSERT(aScript.IsModuleScript());
  MOZ_DIAGNOSTIC_ASSERT(aScript.IsStencil());

  return mozilla::MakeRefPtr<ModuleScript>(aScript).forget();
}

already_AddRefed<LoadedScript> ModuleScript::ToCache() {
  MOZ_DIAGNOSTIC_ASSERT(IsStencil());
  MOZ_DIAGNOSTIC_ASSERT(!HasParseError());
  MOZ_DIAGNOSTIC_ASSERT(!HasErrorToRethrow());

  return mozilla::MakeRefPtr<LoadedScript>(*this).forget();
}

void ModuleScript::Shutdown() {
  if (mModuleRecord) {
    JS::ClearModuleEnvironment(mModuleRecord);
  }

  UnlinkModuleRecord();
}

void ModuleScript::UnlinkModuleRecord() {
  // Remove the module record's pointer to this object if present and decrement
  // our reference count. The reference is added by SetModuleRecord() below.
  //
  if (mModuleRecord) {
    // Take care not to trigger gray unmarking because this takes a lot of time
    // when we're tearing down the entire page. This is safe because we are only
    // writing undefined into the module private, so it won't create any
    // black-gray edges.
    JSObject* module = mModuleRecord.unbarrieredGet();
    if (JS::IsCyclicModule(module)) {
      MOZ_ASSERT(JS::GetModulePrivate(module).toPrivate() == this);
      JS::ClearModulePrivate(module);
    }
    mModuleRecord = nullptr;
  }
}

ModuleScript::~ModuleScript() {
  // The object may be destroyed without being unlinked first.
  UnlinkModuleRecord();
}

void ModuleScript::SetModuleRecord(JS::Handle<JSObject*> aModuleRecord) {
  MOZ_ASSERT(!mModuleRecord);
  MOZ_ASSERT_IF(IsModuleScript(), !AsModuleScript()->HasParseError());
  MOZ_ASSERT_IF(IsModuleScript(), !AsModuleScript()->HasErrorToRethrow());

  mModuleRecord = aModuleRecord;

  if (JS::IsCyclicModule(mModuleRecord)) {
    // Make module's host defined field point to this object. The JS engine will
    // increment our reference count by calling HostAddRefTopLevelScript(). This
    // is decremented when the field is cleared in UnlinkModuleRecord() above or
    // when the module record dies.
    MOZ_ASSERT(JS::GetModulePrivate(mModuleRecord).isUndefined());
    JS::SetModulePrivate(mModuleRecord, JS::PrivateValue(this));
  }

  mozilla::HoldJSObjects(this);
}

void ModuleScript::SetParseError(const JS::Value& aError) {
  MOZ_ASSERT(!aError.isUndefined());
  MOZ_ASSERT(!HasParseError());
  MOZ_ASSERT(!HasErrorToRethrow());

  UnlinkModuleRecord();
  mParseError = aError;
  mozilla::HoldJSObjects(this);
}

void ModuleScript::SetErrorToRethrow(const JS::Value& aError) {
  MOZ_ASSERT(!aError.isUndefined());

  // This is only called after SetModuleRecord() or SetParseError() so we don't
  // need to call HoldJSObjects() here.
  MOZ_ASSERT(ModuleRecord() || HasParseError());

  mErrorToRethrow = aError;
}

void ModuleScript::SetForPreload(bool aValue) { mForPreload = aValue; }
void ModuleScript::SetHadImportMap(bool aValue) { mHadImportMap = aValue; }

void ModuleScript::SetDebuggerDataInitialized() {
  MOZ_ASSERT(ModuleRecord());
  MOZ_ASSERT(!mDebuggerDataInitialized);

  mDebuggerDataInitialized = true;
}

}  // namespace JS::loader
