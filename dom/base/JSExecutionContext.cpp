/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is not a generated file. It contains common utility functions
 * invoked from the JavaScript code generated from IDL interfaces.
 * The goal of the utility functions is to cut down on the size of
 * the generated code itself.
 */

#include "mozilla/dom/JSExecutionContext.h"

#include <utility>
#include "ErrorList.h"
#include "MainThreadUtils.h"
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"
#include "js/Conversions.h"
#include "js/experimental/JSStencil.h"
#include "js/HeapAPI.h"
#include "js/ProfilingCategory.h"
#include "js/Promise.h"
#include "js/SourceText.h"
#include "js/Transcoding.h"
#include "js/Value.h"
#include "js/Wrapper.h"
#include "jsapi.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/ScriptLoadContext.h"
#include "mozilla/Likely.h"
#include "nsContentUtils.h"
#include "nsTPromiseFlatString.h"
#include "xpcpublic.h"

#if !defined(DEBUG) && !defined(MOZ_ENABLE_JS_DUMP)
#  include "mozilla/StaticPrefs_browser.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla::dom {

nsresult EvaluationExceptionToNSResult(ErrorResult& aRv) {
  if (aRv.IsJSContextException()) {
    aRv.SuppressException();
    return NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW;
  }
  if (aRv.IsUncatchableException()) {
    aRv.SuppressException();
    return NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW_UNCATCHABLE;
  }
  if (aRv.ErrorCodeIs(NS_ERROR_DOM_NOT_ALLOWED_ERR)) {
    aRv.SuppressException();
    return NS_OK;
  }
  // cases like NS_OK, NS_ERROR_DOM_JS_DECODING_ERROR and NS_ERROR_OUT_OF_MEMORY
  return aRv.StealNSResult();
}

}  // namespace mozilla::dom

JSExecutionContext::JSExecutionContext(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal,
    JS::CompileOptions& aCompileOptions, ErrorResult& aRv,
    JS::Handle<JS::Value> aDebuggerPrivateValue,
    JS::Handle<JSScript*> aDebuggerIntroductionScript)
    : mAutoProfilerLabel("JSExecutionContext",
                         /* dynamicStr */ nullptr,
                         JS::ProfilingCategoryPair::JS),
      mCx(aCx),
      mRealm(aCx, aGlobal),
      mRetValue(aCx),
      mScript(aCx),
      mCompileOptions(aCompileOptions),
      mDebuggerPrivateValue(aCx, aDebuggerPrivateValue),
      mDebuggerIntroductionScript(aCx, aDebuggerIntroductionScript),
      mSkip(false),
      mCoerceToString(false),
      mEncodeBytecode(false)
#ifdef DEBUG
      ,
      mWantsReturnValue(false),
      mScriptUsed(false)
#endif
{
  MOZ_ASSERT(aCx == nsContentUtils::GetCurrentJSContext());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(CycleCollectedJSContext::Get() &&
             CycleCollectedJSContext::Get()->MicroTaskLevel());
  MOZ_ASSERT(mRetValue.isUndefined());

  MOZ_ASSERT(JS_IsGlobalObject(aGlobal));
  if (MOZ_UNLIKELY(!xpc::Scriptability::Get(aGlobal).Allowed())) {
    mSkip = true;
    aRv = NS_ERROR_DOM_NOT_ALLOWED_ERR;
  }
}

void JSExecutionContext::JoinOffThread(ScriptLoadContext* aContext,
                                       ErrorResult& aRv) {
  MOZ_ASSERT(!mSkip);

  MOZ_ASSERT(!mWantsReturnValue);

  JS::InstantiationStorage storage;
  RefPtr<JS::Stencil> stencil = aContext->StealOffThreadResult(mCx, &storage);
  if (!stencil) {
    mSkip = true;
    aRv.NoteJSContextException(mCx);
    return;
  }

  if (mKeepStencil) {
    mStencil = JS::DuplicateStencil(mCx, stencil.get());
    if (!mStencil) {
      mSkip = true;
      aRv.NoteJSContextException(mCx);
      return;
    }
  }

  bool unused;
  InstantiateStencil(std::move(stencil), unused, aRv, &storage);
}

template <typename Unit>
void JSExecutionContext::InternalCompile(JS::SourceText<Unit>& aSrcBuf,
                                         ErrorResult& aRv) {
  MOZ_ASSERT(!mSkip);

  MOZ_ASSERT(aSrcBuf.get());
  MOZ_ASSERT(mRetValue.isUndefined());
#ifdef DEBUG
  mWantsReturnValue = !mCompileOptions.noScriptRval;
#endif

  RefPtr<JS::Stencil> stencil =
      CompileGlobalScriptToStencil(mCx, mCompileOptions, aSrcBuf);
  if (!stencil) {
    mSkip = true;
    aRv.NoteJSContextException(mCx);
    return;
  }

  if (mKeepStencil) {
    mStencil = JS::DuplicateStencil(mCx, stencil.get());
    if (!mStencil) {
      mSkip = true;
      aRv.NoteJSContextException(mCx);
      return;
    }
  }

  bool unused;
  InstantiateStencil(std::move(stencil), unused, aRv);
}

void JSExecutionContext::Compile(JS::SourceText<char16_t>& aSrcBuf,
                                 ErrorResult& aRv) {
  InternalCompile(aSrcBuf, aRv);
}

void JSExecutionContext::Compile(JS::SourceText<Utf8Unit>& aSrcBuf,
                                 ErrorResult& aRv) {
  InternalCompile(aSrcBuf, aRv);
}

void JSExecutionContext::Compile(const nsAString& aScript, ErrorResult& aRv) {
  MOZ_ASSERT(!mSkip);

  const nsPromiseFlatString& flatScript = PromiseFlatString(aScript);
  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(mCx, flatScript.get(), flatScript.Length(),
                   JS::SourceOwnership::Borrowed)) {
    mSkip = true;
    aRv.NoteJSContextException(mCx);
    return;
  }

  Compile(srcBuf, aRv);
}

void JSExecutionContext::Decode(const JS::TranscodeRange& aBytecodeBuf,
                                ErrorResult& aRv) {
  MOZ_ASSERT(!mSkip);

  JS::DecodeOptions decodeOptions(mCompileOptions);
  decodeOptions.borrowBuffer = true;

  MOZ_ASSERT(!mWantsReturnValue);
  RefPtr<JS::Stencil> stencil;
  JS::TranscodeResult tr = JS::DecodeStencil(mCx, decodeOptions, aBytecodeBuf,
                                             getter_AddRefs(stencil));
  // These errors are external parameters which should be handled before the
  // decoding phase, and which are the only reasons why you might want to
  // fallback on decoding failures.
  MOZ_ASSERT(tr != JS::TranscodeResult::Failure_BadBuildId);
  if (tr != JS::TranscodeResult::Ok) {
    mSkip = true;
    aRv = NS_ERROR_DOM_JS_DECODING_ERROR;
    return;
  }

  if (mKeepStencil) {
    mStencil = JS::DuplicateStencil(mCx, stencil.get());
    if (!mStencil) {
      mSkip = true;
      aRv.NoteJSContextException(mCx);
      return;
    }
  }

  bool unused;
  InstantiateStencil(std::move(stencil), unused, aRv);
}

void JSExecutionContext::InstantiateStencil(
    RefPtr<JS::Stencil>&& aStencil, bool& incrementalEncodingAlreadyStarted,
    ErrorResult& aRv, JS::InstantiationStorage* aStorage) {
  JS::InstantiateOptions instantiateOptions(mCompileOptions);
  JS::Rooted<JSScript*> script(
      mCx, JS::InstantiateGlobalStencil(mCx, instantiateOptions, aStencil,
                                        aStorage));
  if (!script) {
    mSkip = true;
    aRv.NoteJSContextException(mCx);
    return;
  }

  if (mEncodeBytecode) {
    if (!JS::StartIncrementalEncoding(mCx, std::move(aStencil),
                                      incrementalEncodingAlreadyStarted)) {
      mSkip = true;
      aRv.NoteJSContextException(mCx);
      return;
    }
  }

  MOZ_ASSERT(!mScript);
  mScript.set(script);

  if (instantiateOptions.deferDebugMetadata) {
    if (!JS::UpdateDebugMetadata(mCx, mScript, instantiateOptions,
                                 mDebuggerPrivateValue, nullptr,
                                 mDebuggerIntroductionScript, nullptr)) {
      aRv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
}

JSScript* JSExecutionContext::GetScript() {
#ifdef DEBUG
  MOZ_ASSERT(!mSkip);
  MOZ_ASSERT(mScript);
  mScriptUsed = true;
#endif

  return MaybeGetScript();
}

JSScript* JSExecutionContext::MaybeGetScript() { return mScript; }

void JSExecutionContext::ExecScript(ErrorResult& aRv) {
  MOZ_ASSERT(!mSkip);

  MOZ_ASSERT(mScript);

  if (!JS_ExecuteScript(mCx, mScript)) {
    mSkip = true;
    aRv.NoteJSContextException(mCx);
  }
}

static bool IsPromiseValue(JSContext* aCx, JS::Handle<JS::Value> aValue) {
  if (!aValue.isObject()) {
    return false;
  }

  // We only care about Promise here, so CheckedUnwrapStatic is fine.
  JS::Rooted<JSObject*> obj(aCx, js::CheckedUnwrapStatic(&aValue.toObject()));
  if (!obj) {
    return false;
  }

  return JS::IsPromiseObject(obj);
}

void JSExecutionContext::ExecScript(JS::MutableHandle<JS::Value> aRetValue,
                                    ErrorResult& aRv) {
  MOZ_ASSERT(!mSkip);

  MOZ_ASSERT(mScript);
  MOZ_ASSERT(mWantsReturnValue);

  if (!JS_ExecuteScript(mCx, mScript, aRetValue)) {
    mSkip = true;
    aRv.NoteJSContextException(mCx);
    return;
  }

#ifdef DEBUG
  mWantsReturnValue = false;
#endif
  if (mCoerceToString && IsPromiseValue(mCx, aRetValue)) {
    // We're a javascript: url and we should treat Promise return values as
    // undefined.
    //
    // Once bug 1477821 is fixed this code might be able to go away, or will
    // become enshrined in the spec, depending.
    aRetValue.setUndefined();
  }

  if (mCoerceToString && !aRetValue.isUndefined()) {
    JSString* str = JS::ToString(mCx, aRetValue);
    if (!str) {
      // ToString can be a function call, so an exception can be raised while
      // executing the function.
      mSkip = true;
      aRv.NoteJSContextException(mCx);
      return;
    }
    aRetValue.set(JS::StringValue(str));
  }
}
