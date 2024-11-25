/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UniFFICall_h
#define mozilla_UniFFICall_h

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/OwnedRustBuffer.h"
#include "mozilla/dom/UniFFIRust.h"
#include "mozilla/dom/UniFFIScaffolding.h"

namespace mozilla::uniffi {

// Call Rust scaffolding functions
//
// This is the base of a class hierarchy for Rust call handling:
//   - UniffiCallHandlerBase contains the shared code for both async and sync
//   calls.
//   - UniffiSyncCallHandler and UniffiAsyncCallHandler contain generalized
//     code for sync/async calls
//   - The generated code creates subclasses one of the those and implements
//     the specialized code for the call.
//
// In all cases, a new instance is created each time the scaffolding function
// is called.
class UniffiCallHandlerBase {
 protected:
  // Extract the call result when the status code is `RUST_CALL_SUCCESS`.
  //
  // On success, set aDest with the converted return value. If there is a
  // conversion error, set `aError`.  This can happen for example when a u64
  // value doesn't fit into a JS number.
  //
  // Called on the main thread.
  virtual void ExtractSuccessfulCallResult(
      JSContext* aCx, dom::Optional<dom::UniFFIScaffoldingValue>& aDest,
      ErrorResult& aError) = 0;

  // Extract the result of making a call, and store it into aDest
  //
  // Errors are handled in several different ways:
  //   - If the Rust code returned an `Err` value, then `aDest.code` will be set
  //     to "error" and `aDest.data` will be set to the serialized error value.
  //   - If some other error happens in the Rust layer, then `aDest.code` will
  //     be set to "internal-error" and `aDest.data` will contain a serialized
  //     error string if possible and be empty otherwise.  This should be fairly
  //     rare, since the main case is a caught Rust panic, but FF sets
  //     panic=abort.
  //   - If some other error happens in the C++ layer, then `aError` will be set
  //     to the error.
  void ExtractCallResult(
      JSContext* aCx,
      dom::RootedDictionary<dom::UniFFIScaffoldingCallResult>& aDest,
      ErrorResult& aError);

  // Call status from the rust call
  int8_t mUniffiCallStatusCode = RUST_CALL_SUCCESS;
  OwnedRustBuffer mUniffiCallStatusErrorBuf;
};

// Call scaffolding functions for synchronous Rust calls
class UniffiSyncCallHandler : public UniffiCallHandlerBase {
 protected:
  // ---- Overridden by subclasses for each call ----

  // Convert a sequence of JS arguments and store them in this
  // UniffiSyncCallHandler. Called on the main thread.
  virtual void PrepareRustArgs(
      const dom::Sequence<dom::UniFFIScaffoldingValue>& aArgs,
      ErrorResult& aError) = 0;

  // Call the underlying rust scaffolding function, using the arguments
  // stored in this UniffiHandler, and store the result as a member.
  // Potentially called on a background thread.
  //
  // aOutStatus is the out pointer passed to Rust.  The caller is responible
  // for using it to update `mUniffiCallStatusCode` and
  // `mUniffiCallStatusErrorBuf`.
  virtual void MakeRustCall(RustCallStatus* aOutStatus) = 0;

 public:
  virtual ~UniffiSyncCallHandler() = default;

  // ---- Generic entry points ----

  // Call the function synchronously
  static void CallSync(
      UniquePtr<UniffiSyncCallHandler> aHandler,
      const dom::GlobalObject& aGlobal,
      const dom::Sequence<dom::UniFFIScaffoldingValue>& aArgs,
      dom::RootedDictionary<dom::UniFFIScaffoldingCallResult>& aReturnValue,
      ErrorResult& aError);

  // Call the function asynchronously, in a worker queue.
  static already_AddRefed<dom::Promise> CallAsyncWrapper(
      UniquePtr<UniffiSyncCallHandler> aHandler,
      const dom::GlobalObject& aGlobal,
      const dom::Sequence<dom::UniFFIScaffoldingValue>& aArgs,
      ErrorResult& aError);
};

// Call scaffolding functions for asynchronous Rust calls
class UniffiAsyncCallHandler : public UniffiCallHandlerBase {
 public:
  UniffiAsyncCallHandler(PollFutureFn aPollFn, FreeFutureFn aFreeFn)
      : mPollFn(aPollFn), mFreeFn(aFreeFn) {}

 protected:
  // ---- Overridden by subclasses for each call ----

  // Convert a sequence of JS arguments and call the Rust scaffolding function.
  //
  // Always called on the main thread since async Rust calls don't block, they
  // return a future.  Because of this, there's no reason to split out the
  // `PrepareRustArgs` and `PrepareArgs` and `MakeRustCall` like in the sync
  // case.
  virtual void PrepareArgsAndMakeRustCall(
      const dom::Sequence<dom::UniFFIScaffoldingValue>& aArgs,
      ErrorResult& aError) = 0;

  // Handle to the future we're polling, set by MakeRustCall
  uint64_t mFutureHandle;
  // Rust future function pointers
  PollFutureFn mPollFn;
  FreeFutureFn mFreeFn;
  // Call the complete function.
  //
  // This can't be a function pointer like poll/free since the complete
  // function signature varies based on the return type.
  //
  // aOutStatus is the out pointer passed to Rust.  The caller is responible
  // for using it to update `mUniffiCallStatusCode` and
  // `mUniffiCallStatusErrorBuf`.
  virtual void CallCompleteFn(RustCallStatus* aOutStatus) = 0;

  // Call mPollFn to poll the future
  static void Poll(UniquePtr<UniffiAsyncCallHandler> aHandler);

  // Complete the async call and resolve the promise returned by CallAsync
  //
  // Called in the main thread.
  static void Finish(UniquePtr<UniffiAsyncCallHandler> aHandler);

 public:
  virtual ~UniffiAsyncCallHandler();

  // ---- Generic entry points ----

  // Call the function asynchronously
  static already_AddRefed<dom::Promise> CallAsync(
      UniquePtr<UniffiAsyncCallHandler> aHandler,
      const dom::GlobalObject& aGlobal,
      const dom::Sequence<dom::UniFFIScaffoldingValue>& aArgs,
      ErrorResult& aError);

 private:
  // Promise created by CallAsync
  RefPtr<dom::Promise> mPromise;

  // Callback function for Rust async calls
  //
  // This is passed to Rust when we call the poll function.  The callback is
  // called when the future is ready or it's waker is invoked.  aCode is used to
  // distinguish the two cases.
  //
  // This is static so we can pass it as a function pointer to Rust.
  static void FutureCallback(uint64_t aCallHandlerHandle, int8_t aCode);
};

}  // namespace mozilla::uniffi

#endif  // mozilla_UniFFICall_h
