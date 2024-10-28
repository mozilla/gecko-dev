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
  aHandler->PrepareRustArgs(aArgs, aError);
  if (aError.Failed()) {
    return;
  }
  aHandler->MakeRustCall();
  aHandler->ExtractCallResult(aGlobal.Context(), aReturnValue, aError);
}

already_AddRefed<dom::Promise> UniffiSyncCallHandler::CallAsyncWrapper(
    UniquePtr<UniffiSyncCallHandler> aHandler, const dom::GlobalObject& aGlobal,
    const dom::Sequence<dom::UniFFIScaffoldingValue>& aArgs,
    ErrorResult& aError) {
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
            handler->MakeRustCall();
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
        dom::AutoEntryScript aes(xpcomGlobal, __func__);
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

void UniffiSyncCallHandler::ExtractCallResult(
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

}  // namespace mozilla::uniffi
