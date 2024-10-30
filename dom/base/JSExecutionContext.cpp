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

#include <utility>      // std::move
#include "ErrorList.h"  // NS_ERROR_OUT_OF_MEMORY, NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW, NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW_UNCATCHABLE
#include "js/CompilationAndEvaluation.h"  // JS::UpdateDebugMetadata
#include "js/experimental/JSStencil.h"    // JS::StartIncrementalEncoding
#include "js/SourceText.h"                // JS::SourceText, JS::SourceOwnership
#include "jsapi.h"                        // JS_IsExceptionPending
#include "nsTPromiseFlatString.h"         // PromiseFlatString

#if !defined(DEBUG) && !defined(MOZ_ENABLE_JS_DUMP)
#  include "mozilla/StaticPrefs_browser.h"
#endif

using namespace mozilla;

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
  // cases like NS_OK, NS_ERROR_DOM_JS_DECODING_ERROR and NS_ERROR_OUT_OF_MEMORY
  return aRv.StealNSResult();
}

void Compile(JSContext* aCx, JS::CompileOptions& aCompileOptions,
             const nsAString& aScript, RefPtr<JS::Stencil>& aStencil,
             ErrorResult& aRv) {
  const nsPromiseFlatString& flatScript = PromiseFlatString(aScript);
  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(aCx, flatScript.get(), flatScript.Length(),
                   JS::SourceOwnership::Borrowed)) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  aStencil = CompileGlobalScriptToStencil(aCx, aCompileOptions, srcBuf);
  if (!aStencil) {
    aRv.NoteJSContextException(aCx);
  }
}

void InstantiateStencil(JSContext* aCx, JS::CompileOptions& aCompileOptions,
                        RefPtr<JS::Stencil>& aStencil,
                        JS::MutableHandle<JSScript*> aScript,
                        ErrorResult& aRv) {
  MOZ_ASSERT(!JS::InstantiateOptions(aCompileOptions).deferDebugMetadata);
  MOZ_ASSERT(!aScript);

  JS::InstantiateOptions instantiateOptions(aCompileOptions);
  aScript.set(
      JS::InstantiateGlobalStencil(aCx, instantiateOptions, aStencil, nullptr));
  if (!aScript) {
    aRv.NoteJSContextException(aCx);
  }
}

void InstantiateStencil(JSContext* aCx, JS::CompileOptions& aCompileOptions,
                        RefPtr<JS::Stencil>&& aStencil,
                        JS::MutableHandle<JSScript*> aScript,
                        bool& incrementalEncodingAlreadyStarted,
                        JS::Handle<JS::Value> aDebuggerPrivateValue,
                        JS::Handle<JSScript*> aDebuggerIntroductionScript,
                        ErrorResult& aRv, bool aEncodeBytecode /* = false */,
                        JS::InstantiationStorage* aStorage) {
  JS::InstantiateOptions instantiateOptions(aCompileOptions);
  JS::Rooted<JSScript*> script(
      aCx, JS::InstantiateGlobalStencil(aCx, instantiateOptions, aStencil,
                                        aStorage));
  if (!script) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  if (aEncodeBytecode) {
    if (!JS::StartIncrementalEncoding(aCx, std::move(aStencil),
                                      incrementalEncodingAlreadyStarted)) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }

  MOZ_ASSERT(!aScript);
  aScript.set(script);

  if (instantiateOptions.deferDebugMetadata) {
    if (!JS::UpdateDebugMetadata(aCx, aScript, instantiateOptions,
                                 aDebuggerPrivateValue, nullptr,
                                 aDebuggerIntroductionScript, nullptr)) {
      aRv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
}

}  // namespace mozilla::dom
