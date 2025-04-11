/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/uniffi/Call.h"
#include "mozilla/uniffi/ResultPromise.h"

namespace mozilla::uniffi {

void ResultPromise::Init(const dom::GlobalObject& aGlobal,
                         ErrorResult& aError) {
  nsCOMPtr<nsIGlobalObject> xpcomGlobal =
      do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<dom::Promise> promise = dom::Promise::Create(xpcomGlobal, aError);
  if (aError.Failed()) {
    return;
  }
  mPromise =
      new nsMainThreadPtrHolder<dom::Promise>("uniffi::ResultPromise", promise);
}

void ResultPromise::Complete(UniquePtr<UniffiCallHandlerBase> aHandler) {
  MOZ_ASSERT(mPromise);
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "uniffi::ResultPromise::Complete",
      [promise = mPromise, handler = std::move(aHandler)]() {
        dom::AutoEntryScript aes(promise->GetGlobalObject(),
                                 "uniffi::ResultPromise::Complete");
        dom::RootedDictionary<dom::UniFFIScaffoldingCallResult> returnValue(
            aes.cx());

        ErrorResult error;
        handler->LiftCallResult(aes.cx(), returnValue, error);
        error.WouldReportJSException();
        if (error.Failed()) {
          promise->MaybeReject(std::move(error));
        } else {
          promise->MaybeResolve(returnValue);
        }
      }));
}

void ResultPromise::RejectWithUnexpectedError() {
  MOZ_ASSERT(mPromise);
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "uniffi::ResultPromise::RejectWithUnexpectedError", [promise = mPromise] {
        promise->MaybeRejectWithUnknownError(
            "UniFFI Unexpected Internal Error");
      }));
}

}  // namespace mozilla::uniffi
