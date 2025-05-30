/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "mozilla/uniffi/OwnedRustBuffer.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/UniFFIBinding.h"
#include "mozilla/uniffi/Callbacks.h"
#include "mozilla/Maybe.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla::uniffi {
extern mozilla::LazyLogModule gUniffiLogger;

void AsyncCallbackMethodHandlerBase::ScheduleAsyncCall(
    UniquePtr<AsyncCallbackMethodHandlerBase> aHandler,
    StaticRefPtr<dom::UniFFICallbackHandler>* aJsHandler) {
  nsresult dispatchResult = NS_DispatchToMainThread(NS_NewRunnableFunction(
      "UniFFI callback", [handler = std::move(aHandler),
                          aJsHandler]() MOZ_CAN_RUN_SCRIPT_BOUNDARY mutable {
        auto reportError = MakeScopeExit([&handler] {
          RootedDictionary<UniFFIScaffoldingCallResult> callResult(
              dom::RootingCx());
          callResult.mCode = dom::UniFFIScaffoldingCallCode::Internal_error;
          handler->HandleReturn(callResult, IgnoreErrors());
        });

        // Take our own reference to the callback handler to ensure that it
        // stays alive for the duration of this call
        RefPtr<dom::UniFFICallbackHandler> jsHandler = *aJsHandler;
        if (!jsHandler) {
          MOZ_LOG(gUniffiLogger, LogLevel::Error,
                  ("[%s] called, but JS handler not registered",
                   handler->mUniffiMethodName));
          return;
        }

        JSObject* global = jsHandler->CallbackGlobalOrNull();
        if (!global) {
          MOZ_LOG(
              gUniffiLogger, LogLevel::Error,
              ("[%s] JS handler has null global", handler->mUniffiMethodName));
          return;
        }

        dom::AutoEntryScript aes(global, handler->mUniffiMethodName);

        IgnoredErrorResult error;
        RefPtr<dom::Promise> promise =
            handler->MakeCall(aes.cx(), jsHandler, error);
        if (error.Failed()) {
          MOZ_LOG(
              gUniffiLogger, LogLevel::Error,
              ("[%s] Error invoking JS handler", handler->mUniffiMethodName));
          return;
        }

        reportError.release();
        if (promise) {
          auto promiseHandler = MakeRefPtr<PromiseHandler>(std::move(handler));
          promise->AppendNativeHandler(promiseHandler);
        }
      }));

  if (NS_FAILED(dispatchResult)) {
    MOZ_LOG(gUniffiLogger, LogLevel::Error,
            ("[UniFFI] Error dispatching UniFFI callback task"));
  }
}

MOZ_CAN_RUN_SCRIPT
already_AddRefed<dom::Promise> CallbackFreeHandler::MakeCall(
    JSContext* aCx, dom::UniFFICallbackHandler* aJsHandler,
    ErrorResult& aError) {
  aJsHandler->Destroy(mUniffiHandle.IntoRust(), aError);
  // CallbackFreeHandler works like a fire-and-forget callback and returns
  // nullptr.  There's no Rust code that's awaiting this result.
  return nullptr;
}

void AsyncCallbackMethodHandlerBase::PromiseHandler::ResolvedCallback(
    JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  RootedDictionary<UniFFIScaffoldingCallResult> callResult(aCx);
  if (!callResult.Init(aCx, aValue)) {
    JS_ClearPendingException(aCx);
    MOZ_LOG(
        gUniffiLogger, LogLevel::Error,
        ("[%s] callback method did not return a UniFFIScaffoldingCallResult",
         mHandler->mUniffiMethodName));
    callResult.mCode = dom::UniFFIScaffoldingCallCode::Internal_error;
  }
  mHandler->HandleReturn(callResult, aRv);
}

void AsyncCallbackMethodHandlerBase::PromiseHandler::RejectedCallback(
    JSContext* aCx, JS::Handle<JS::Value>, ErrorResult& aRv) {
  RootedDictionary<UniFFIScaffoldingCallResult> callResult(aCx);
  callResult.mCode = dom::UniFFIScaffoldingCallCode::Internal_error;
  mHandler->HandleReturn(callResult, aRv);
}

}  // namespace mozilla::uniffi
