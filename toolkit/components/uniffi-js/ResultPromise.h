/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UniFFIResultPromise_h
#define mozilla_dom_UniFFIResultPromise_h

#include "nsThreadUtils.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/UniFFIBinding.h"

namespace mozilla::uniffi {
extern mozilla::LazyLogModule gUniffiLogger;

class UniffiAsyncCallHandler;
class UniffiCallHandlerBase;

/**
 * JS "bridge" for UniFFI
 *
 * This module intefaces with the SpiderMonkey JS API so that other code can
 * focus on the core UniFFI logic.
 */

class ResultPromise {
 public:
  ResultPromise() = default;

  // Initialize a ResultPromise, this must be done before calling any other
  // methods.
  //
  // This must be called on the main thread.
  void Init(const dom::GlobalObject& aGlobal, ErrorResult& aError);

  // Get a raw `dom::Promise` pointer
  //
  // Use this to return the promise from a webidl-generated function.
  // May only be called on the main thread.
  dom::Promise* GetPromise() { return mPromise; }

  // Complete the promise using a call handler, this can be called from any
  // thread.
  //
  // After a promise is completed, it must not be used anymore.
  void Complete(UniquePtr<UniffiCallHandlerBase> aHandler);

  // Reject the promise with an unexpected error.
  //
  // Use this as a last resort, when something has gone very wrong in the FFI.
  //
  // After a promise is rejected, it must not be used anymore.
  void RejectWithUnexpectedError();

 private:
  // The `nsMainThreadPtrHandle` ensures that if this type is destroyed
  // from off-main-thread, it'll actually be released on the main thread.
  // This is important because the promise is a main-thread-only object.
  nsMainThreadPtrHandle<dom::Promise> mPromise;
};

}  // namespace mozilla::uniffi

#endif  // mozilla_dom_UniFFIResultPromise_h
