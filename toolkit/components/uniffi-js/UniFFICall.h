/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UniFFICall_h
#define mozilla_UniFFICall_h

#include "mozilla/UniquePtr.h"
#include "mozilla/dom/OwnedRustBuffer.h"
#include "mozilla/dom/UniFFIRust.h"
#include "mozilla/dom/UniFFIScaffolding.h"

namespace mozilla::uniffi {

class UniffiSyncCallHandler {
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
  virtual void MakeRustCall() = 0;

  // Call status from the last MakeRustCall
  int8_t mUniffiCallStatusCode = RUST_CALL_SUCCESS;
  OwnedRustBuffer mUniffiCallStatusErrorBuf;

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

  // Extract the result of making a call, and store it in aDestinto aDest
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

 public:
  virtual ~UniffiSyncCallHandler() = default;

  // ---- Generic entry points ----

  // Call a sync function
  static void CallSync(
      UniquePtr<UniffiSyncCallHandler> aHandler,
      const dom::GlobalObject& aGlobal,
      const dom::Sequence<dom::UniFFIScaffoldingValue>& aArgs,
      dom::RootedDictionary<dom::UniFFIScaffoldingCallResult>& aReturnValue,
      ErrorResult& aError);

  // Call a sync function asynchronously, in a worker queue.
  static already_AddRefed<dom::Promise> CallAsyncWrapper(
      UniquePtr<UniffiSyncCallHandler> aHandler,
      const dom::GlobalObject& aGlobal,
      const dom::Sequence<dom::UniFFIScaffoldingValue>& aArgs,
      ErrorResult& aError);
};

}  // namespace mozilla::uniffi

#endif  // mozilla_UniFFICall_h
