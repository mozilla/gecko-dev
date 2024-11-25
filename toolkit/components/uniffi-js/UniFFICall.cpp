/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/UniFFICall.h"

namespace mozilla::uniffi {
extern mozilla::LazyLogModule gUniffiLogger;

using dom::GlobalObject;
using dom::RootedDictionary;
using dom::Sequence;
using dom::UniFFIScaffoldingCallResult;
using dom::UniFFIScaffoldingValue;

void UniffiSyncCallHandler::CallSync(
    UniquePtr<UniffiSyncCallHandler> aHandler, const GlobalObject& aGlobal,
    const Sequence<UniFFIScaffoldingValue>& aArgs,
    RootedDictionary<UniFFIScaffoldingCallResult>& aReturnValue,
    ErrorResult& aError) {
  MOZ_ASSERT(NS_IsMainThread());
  aHandler->PrepareRustArgs(aArgs, aError);
  if (aError.Failed()) {
    return;
  }
  RustCallStatus callStatus{};
  aHandler->MakeRustCall(&callStatus);
  aHandler->mUniffiCallStatusCode = callStatus.code;
  if (callStatus.error_buf.data) {
    aHandler->mUniffiCallStatusErrorBuf = OwnedRustBuffer(callStatus.error_buf);
  }
  aHandler->ExtractCallResult(aGlobal.Context(), aReturnValue, aError);
}

already_AddRefed<dom::Promise> UniffiSyncCallHandler::CallAsyncWrapper(
    UniquePtr<UniffiSyncCallHandler> aHandler, const dom::GlobalObject& aGlobal,
    const dom::Sequence<dom::UniFFIScaffoldingValue>& aArgs,
    ErrorResult& aError) {
  MOZ_ASSERT(NS_IsMainThread());
  aHandler->PrepareRustArgs(aArgs, aError);
  if (aError.Failed()) {
    return nullptr;
  }

  // Create the promise that we return to JS
  nsCOMPtr<nsIGlobalObject> xpcomGlobal =
      do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<dom::Promise> returnPromise =
      dom::Promise::Create(xpcomGlobal, aError);
  if (aError.Failed()) {
    return nullptr;
  }

  // Create a second promise that gets resolved by a background task that
  // calls the scaffolding function
  RefPtr taskPromise =
      new MozPromise<UniquePtr<UniffiSyncCallHandler>, nsresult, true>::Private(
          __func__);

  nsresult dispatchResult = NS_DispatchBackgroundTask(
      NS_NewRunnableFunction(
          __func__,
          [handler = std::move(aHandler), taskPromise]() mutable {
            RustCallStatus callStatus{};
            handler->MakeRustCall(&callStatus);
            handler->mUniffiCallStatusCode = callStatus.code;
            if (callStatus.error_buf.data) {
              handler->mUniffiCallStatusErrorBuf =
                  OwnedRustBuffer(callStatus.error_buf);
            }
            taskPromise->Resolve(std::move(handler), __func__);
          }),
      NS_DISPATCH_EVENT_MAY_BLOCK);
  if (NS_FAILED(dispatchResult)) {
    taskPromise->Reject(dispatchResult, __func__);
  }

  // When the background task promise completes, resolve the JS promise
  taskPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [xpcomGlobal, returnPromise](
          typename MozPromise<UniquePtr<UniffiSyncCallHandler>, nsresult,
                              true>::ResolveOrRejectValue&& aResult) {
        if (!aResult.IsResolve()) {
          returnPromise->MaybeRejectWithUnknownError(__func__);
          return;
        }
        auto handler = std::move(aResult.ResolveValue());
        dom::AutoEntryScript aes(xpcomGlobal,
                                 "UniffiSyncCallHandler::CallAsyncWrapper");
        dom::RootedDictionary<dom::UniFFIScaffoldingCallResult> returnValue(
            aes.cx());

        ErrorResult error;
        handler->ExtractCallResult(aes.cx(), returnValue, error);
        error.WouldReportJSException();
        if (error.Failed()) {
          returnPromise->MaybeReject(std::move(error));
        } else {
          returnPromise->MaybeResolve(returnValue);
        }
      });

  // Return the JS promise, using forget() to convert it to already_AddRefed
  return returnPromise.forget();
}

void UniffiCallHandlerBase::ExtractCallResult(
    JSContext* aCx,
    dom::RootedDictionary<dom::UniFFIScaffoldingCallResult>& aDest,
    ErrorResult& aError) {
  switch (mUniffiCallStatusCode) {
    case RUST_CALL_SUCCESS: {
      aDest.mCode = dom::UniFFIScaffoldingCallCode::Success;
      ExtractSuccessfulCallResult(aCx, aDest.mData, aError);
      break;
    }

    case RUST_CALL_ERROR: {
      // Rust Err() value.  Populate data with the `RustBuffer` containing the
      // error
      aDest.mCode = dom::UniFFIScaffoldingCallCode::Error;

      JS::Rooted<JSObject*> obj(aCx);
      mUniffiCallStatusErrorBuf.IntoArrayBuffer(aCx, &obj, aError);
      if (aError.Failed()) {
        break;
      }
      aDest.mData.Construct().SetAsArrayBuffer().Init(obj);
      break;
    }

    default: {
      // This indicates a RustError, which should rarely happen in practice.
      // The normal case is a Rust panic, but FF sets panic=abort.
      aDest.mCode = dom::UniFFIScaffoldingCallCode::Internal_error;
      if (mUniffiCallStatusErrorBuf.IsValid()) {
        JS::Rooted<JSObject*> obj(aCx);
        mUniffiCallStatusErrorBuf.IntoArrayBuffer(aCx, &obj, aError);
        if (aError.Failed()) {
          break;
        }
        aDest.mData.Construct().SetAsArrayBuffer().Init(obj);
      }

      break;
    }
  }
}

already_AddRefed<dom::Promise> UniffiAsyncCallHandler::CallAsync(
    UniquePtr<UniffiAsyncCallHandler> aHandler,
    const dom::GlobalObject& aGlobal,
    const dom::Sequence<dom::UniFFIScaffoldingValue>& aArgs,
    ErrorResult& aError) {
  MOZ_ASSERT(NS_IsMainThread());
  // Async calls return a Future rather than doing any work.  This means we can
  // make the call right now on the JS main thread without fear of blocking it.
  aHandler->PrepareArgsAndMakeRustCall(aArgs, aError);
  if (aError.Failed()) {
    return nullptr;
  }

  // Create the promise that the handler will resolve
  nsCOMPtr<nsIGlobalObject> global(do_QueryInterface(aGlobal.GetAsSupports()));
  aHandler->mPromise = dom::Promise::Create(global, aError);
  // Also get a copy to return to JS
  RefPtr<dom::Promise> returnPromise(aHandler->mPromise);

  if (aError.Failed()) {
    aError.ThrowUnknownError("[UniFFI] dom::Promise::Create failed"_ns);
    return nullptr;
  }

  // Schedule a poll for the future in a background thread.
  nsresult dispatchResult = NS_DispatchBackgroundTask(NS_NewRunnableFunction(
      __func__, [handler = std::move(aHandler)]() mutable {
        UniffiAsyncCallHandler::Poll(std::move(handler));
      }));
  if (NS_FAILED(dispatchResult)) {
    aError.ThrowUnknownError(
        "[UniFFI] UniffiAsyncCallHandler::CallAsync - Error scheduling background task"_ns);
    return nullptr;
  }

  // Return a copy of the JS promise, using forget() to convert it to
  // already_AddRefed
  return returnPromise.forget();
}

// Callback function for async calls
//
// This is passed to Rust when we poll the future alongside a 64-bit handle that
// represents the callback data.  For uniffi-bindgen-gecko-js, the handle is a
// `UniffiAsyncCallHandler*` casted to an int.
//
// Rust calls this when either the future is ready or when it's time to poll it
// again.
void UniffiAsyncCallHandler::FutureCallback(uint64_t aCallHandlerHandle,
                                            int8_t aCode) {
  // Recreate the UniquePtr we previously released
  UniquePtr<UniffiAsyncCallHandler> handler(
      reinterpret_cast<UniffiAsyncCallHandler*>(
          static_cast<uintptr_t>(aCallHandlerHandle)));

  switch (aCode) {
    case UNIFFI_FUTURE_READY: {
      // `Future::poll` returned a `Ready` value on the Rust side.
      nsresult dispatchResult = NS_DispatchToMainThread(NS_NewRunnableFunction(
          __func__, [handler = std::move(handler)]() mutable {
            UniffiAsyncCallHandler::Finish(std::move(handler));
          }));
      if (NS_FAILED(dispatchResult)) {
        MOZ_LOG(gUniffiLogger, LogLevel::Error,
                ("[UniFFI] NS_DispatchToMainThread failed in "
                 "UniffiAsyncCallHandler::FutureCallback"));
      }
      break;
    }

    case UNIFFI_FUTURE_MAYBE_READY: {
      // The Rust waker was invoked after `poll` returned a `Pending` value.
      // Poll the future again soon in a background task.
      nsresult dispatchResult = NS_DispatchBackgroundTask(
          NS_NewRunnableFunction(__func__,
                                 [handler = std::move(handler)]() mutable {
                                   UniffiAsyncCallHandler::Poll(
                                       std::move(handler));
                                 }),
          NS_DISPATCH_NORMAL);
      if (NS_FAILED(dispatchResult)) {
        MOZ_LOG(gUniffiLogger, LogLevel::Error,
                ("[UniFFI] NS_DispatchBackgroundTask failed in "
                 "UniffiAsyncCallHandler::FutureCallback"));
      }
      break;
    }

    default: {
      // Invalid poll code, this should never happen, but if it does log an
      // error and reject the promise.
      MOZ_LOG(gUniffiLogger, LogLevel::Error,
              ("[UniFFI] Invalid poll code in "
               "UniffiAsyncCallHandler::FutureCallback %d",
               aCode));
      handler->mPromise->MaybeRejectWithUndefined();
      break;
    }
  };
}

void UniffiAsyncCallHandler::Poll(UniquePtr<UniffiAsyncCallHandler> aHandler) {
  auto futureHandle = aHandler->mFutureHandle;
  auto pollFn = aHandler->mPollFn;
  // Release the UniquePtr into a raw pointer and convert it to a handle
  // so that we can pass it as a handle to the UniFFI code.  It gets converted
  // back in `UniffiAsyncCallHandler::FutureCallback()`, which the Rust code
  // guarentees will be called if the future makes progress.
  uint64_t selfHandle =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(aHandler.release()));
  pollFn(futureHandle, UniffiAsyncCallHandler::FutureCallback, selfHandle);
}

// Complete the Rust future, extract the call result and resolve/reject the JS
// promise
void UniffiAsyncCallHandler::Finish(
    UniquePtr<UniffiAsyncCallHandler> aHandler) {
  RefPtr<dom::Promise> promise = aHandler->mPromise;
  if (!promise) {
    return;
  }
  dom::AutoEntryScript aes(promise->GetGlobalObject(),
                           "UniffiAsyncCallHandler::Finish");
  dom::RootedDictionary<dom::UniFFIScaffoldingCallResult> returnValue(aes.cx());
  ErrorResult error;

  RustCallStatus callStatus{};
  aHandler->CallCompleteFn(&callStatus);
  aHandler->mUniffiCallStatusCode = callStatus.code;
  if (callStatus.error_buf.data) {
    aHandler->mUniffiCallStatusErrorBuf = OwnedRustBuffer(callStatus.error_buf);
  }
  aHandler->ExtractCallResult(aes.cx(), returnValue, error);
  error.WouldReportJSException();
  if (error.Failed()) {
    aHandler->mPromise->MaybeReject(std::move(error));
  } else {
    aHandler->mPromise->MaybeResolve(returnValue);
  }
}

UniffiAsyncCallHandler::~UniffiAsyncCallHandler() { mFreeFn(mFutureHandle); }

}  // namespace mozilla::uniffi
