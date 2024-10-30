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
using mozilla::dom::JSExecutionContext;

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
    : mDebuggerPrivateValue(aCx, aDebuggerPrivateValue),
      mDebuggerIntroductionScript(aCx, aDebuggerIntroductionScript),
      mSkip(false) {
  MOZ_ASSERT(aCx == nsContentUtils::GetCurrentJSContext());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(CycleCollectedJSContext::Get() &&
             CycleCollectedJSContext::Get()->MicroTaskLevel());

  MOZ_ASSERT(JS_IsGlobalObject(aGlobal));
  if (MOZ_UNLIKELY(!xpc::Scriptability::Get(aGlobal).Allowed())) {
    mSkip = true;
    aRv = NS_ERROR_DOM_NOT_ALLOWED_ERR;
  }
}

void JSExecutionContext::JoinOffThread(JSContext* aCx,
                                       JS::CompileOptions& aCompileOptions,
                                       ScriptLoadContext* aContext,
                                       JS::MutableHandle<JSScript*> aScript,
                                       ErrorResult& aRv,
                                       bool aEncodeBytecode /* = false */) {
  MOZ_ASSERT(!mSkip);

  MOZ_ASSERT(aCompileOptions.noScriptRval);

  JS::InstantiationStorage storage;
  RefPtr<JS::Stencil> stencil = aContext->StealOffThreadResult(aCx, &storage);
  if (!stencil) {
    mSkip = true;
    aRv.NoteJSContextException(aCx);
    return;
  }

  if (mKeepStencil) {
    mStencil = JS::DuplicateStencil(aCx, stencil.get());
    if (!mStencil) {
      mSkip = true;
      aRv.NoteJSContextException(aCx);
      return;
    }
  }

  bool unused;
  InstantiateStencil(aCx, aCompileOptions, std::move(stencil), aScript, unused,
                     aRv, aEncodeBytecode, &storage);
}

template <typename Unit>
void JSExecutionContext::InternalCompile(JSContext* aCx,
                                         JS::CompileOptions& aCompileOptions,
                                         JS::SourceText<Unit>& aSrcBuf,
                                         JS::MutableHandle<JSScript*> aScript,
                                         bool aEncodeBytecode,
                                         ErrorResult& aRv) {
  MOZ_ASSERT(!mSkip);

  MOZ_ASSERT(aSrcBuf.get());

  RefPtr<JS::Stencil> stencil =
      CompileGlobalScriptToStencil(aCx, aCompileOptions, aSrcBuf);
  if (!stencil) {
    mSkip = true;
    aRv.NoteJSContextException(aCx);
    return;
  }

  if (mKeepStencil) {
    mStencil = JS::DuplicateStencil(aCx, stencil.get());
    if (!mStencil) {
      mSkip = true;
      aRv.NoteJSContextException(aCx);
      return;
    }
  }

  bool unused;
  InstantiateStencil(aCx, aCompileOptions, std::move(stencil), aScript, unused,
                     aRv, aEncodeBytecode);
}

void JSExecutionContext::Compile(JSContext* aCx,
                                 JS::CompileOptions& aCompileOptions,
                                 JS::SourceText<char16_t>& aSrcBuf,
                                 JS::MutableHandle<JSScript*> aScript,
                                 ErrorResult& aRv,
                                 bool aEncodeBytecode /*= false */) {
  InternalCompile(aCx, aCompileOptions, aSrcBuf, aScript, aEncodeBytecode, aRv);
}

void JSExecutionContext::Compile(JSContext* aCx,
                                 JS::CompileOptions& aCompileOptions,
                                 JS::SourceText<Utf8Unit>& aSrcBuf,
                                 JS::MutableHandle<JSScript*> aScript,
                                 ErrorResult& aRv,
                                 bool aEncodeBytecode /*= false */) {
  InternalCompile(aCx, aCompileOptions, aSrcBuf, aScript, aEncodeBytecode, aRv);
}

void JSExecutionContext::Compile(JSContext* aCx,
                                 JS::CompileOptions& aCompileOptions,
                                 const nsAString& aScript,
                                 JS::MutableHandle<JSScript*> aScriptOut,
                                 ErrorResult& aRv,
                                 bool aEncodeBytecode /*= false */) {
  MOZ_ASSERT(!mSkip);

  const nsPromiseFlatString& flatScript = PromiseFlatString(aScript);
  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(aCx, flatScript.get(), flatScript.Length(),
                   JS::SourceOwnership::Borrowed)) {
    mSkip = true;
    aRv.NoteJSContextException(aCx);
    return;
  }

  Compile(aCx, aCompileOptions, srcBuf, aScriptOut, aRv, aEncodeBytecode);
}

void JSExecutionContext::Decode(JSContext* aCx,
                                JS::CompileOptions& aCompileOptions,
                                const JS::TranscodeRange& aBytecodeBuf,
                                JS::MutableHandle<JSScript*> aScript,
                                ErrorResult& aRv) {
  MOZ_ASSERT(!mSkip);

  JS::DecodeOptions decodeOptions(aCompileOptions);
  decodeOptions.borrowBuffer = true;

  MOZ_ASSERT(aCompileOptions.noScriptRval);
  RefPtr<JS::Stencil> stencil;
  JS::TranscodeResult tr = JS::DecodeStencil(aCx, decodeOptions, aBytecodeBuf,
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
    mStencil = JS::DuplicateStencil(aCx, stencil.get());
    if (!mStencil) {
      mSkip = true;
      aRv.NoteJSContextException(aCx);
      return;
    }
  }

  bool unused;
  InstantiateStencil(aCx, aCompileOptions, std::move(stencil), aScript, unused,
                     aRv);
}

void JSExecutionContext::InstantiateStencil(
    JSContext* aCx, JS::CompileOptions& aCompileOptions,
    RefPtr<JS::Stencil>&& aStencil, JS::MutableHandle<JSScript*> aScript,
    bool& incrementalEncodingAlreadyStarted, ErrorResult& aRv,
    bool aEncodeBytecode /* = false */, JS::InstantiationStorage* aStorage) {
  JS::InstantiateOptions instantiateOptions(aCompileOptions);
  JS::Rooted<JSScript*> script(
      aCx, JS::InstantiateGlobalStencil(aCx, instantiateOptions, aStencil,
                                        aStorage));
  if (!script) {
    mSkip = true;
    aRv.NoteJSContextException(aCx);
    return;
  }

  if (aEncodeBytecode) {
    if (!JS::StartIncrementalEncoding(aCx, std::move(aStencil),
                                      incrementalEncodingAlreadyStarted)) {
      mSkip = true;
      aRv.NoteJSContextException(aCx);
      return;
    }
  }

  MOZ_ASSERT(!aScript);
  aScript.set(script);

  if (instantiateOptions.deferDebugMetadata) {
    if (!JS::UpdateDebugMetadata(aCx, aScript, instantiateOptions,
                                 mDebuggerPrivateValue, nullptr,
                                 mDebuggerIntroductionScript, nullptr)) {
      aRv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
}

namespace mozilla::dom {

void ExecScript(JSContext* aCx, JS::Handle<JSScript*> aScript,
                ErrorResult& aRv) {
  MOZ_ASSERT(aScript);

  if (!JS_ExecuteScript(aCx, aScript)) {
    aRv.NoteJSContextException(aCx);
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

void ExecScript(JSContext* aCx, JS::Handle<JSScript*> aScript,
                JS::MutableHandle<JS::Value> aRetValue, ErrorResult& aRv,
                bool aCoerceToString /* = false */) {
  MOZ_ASSERT(aScript);

  if (!JS_ExecuteScript(aCx, aScript, aRetValue)) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  if (aCoerceToString && IsPromiseValue(aCx, aRetValue)) {
    // We're a javascript: url and we should treat Promise return values as
    // undefined.
    //
    // Once bug 1477821 is fixed this code might be able to go away, or will
    // become enshrined in the spec, depending.
    aRetValue.setUndefined();
  }

  if (aCoerceToString && !aRetValue.isUndefined()) {
    JSString* str = JS::ToString(aCx, aRetValue);
    if (!str) {
      // ToString can be a function call, so an exception can be raised while
      // executing the function.
      aRv.NoteJSContextException(aCx);
      return;
    }
    aRetValue.set(JS::StringValue(str));
  }
}

}  // namespace mozilla::dom
