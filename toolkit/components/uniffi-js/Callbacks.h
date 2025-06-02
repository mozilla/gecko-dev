/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UniFFICallbacks_h
#define mozilla_UniFFICallbacks_h

#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/UniFFIScaffolding.h"
#include "mozilla/uniffi/FfiValue.h"
#include "mozilla/uniffi/Rust.h"

namespace mozilla::uniffi {

/**
 * Generated code to register a callback handler.
 *
 * This stores a reference to JS callback handler.  When Rust wants to invoke a
 * callback method, we will use this reference.
 *
 * Also, call the Rust FFI function to initialize the callback interface.
 */
void RegisterCallbackHandler(uint64_t aInterfaceId,
                             dom::UniFFICallbackHandler& aCallbackHandler,
                             ErrorResult& aError);

/**
 * Generated code to deregister a callback handler.
 *
 * This releases the reference to the JS callback handler. After this, our
 * vtable will still be registered with Rust, but all method calls will fail.
 */
void DeregisterCallbackHandler(uint64_t aInterfaceId, ErrorResult& aError);

/**
 * Base class for async callback interface method handlers
 *
 * In addition to handling actual async methods this also handles
 * fire-and-forget methods.  These are sync methods wrapped to be async, where
 * we ignore the return value.
 *
 * The generated subclass handles the specifics of each call, while the code in
 * the base class handles generic aspects of the call
 *
 * The generated subclass stores all data needed to make the call, including the
 * arguments passed from Rust internally. MakeCall must only be called
 * once-per-object, since it may consume some of the arguments. We create a new
 * UniffiCallbackMethodHandlerBase subclass instance for each callback interface
 * call from Rust.
 */
class AsyncCallbackMethodHandlerBase {
 public:
  AsyncCallbackMethodHandlerBase(const char* aUniffiMethodName,
                                 uint64_t aUniffiHandle)
      : mUniffiMethodName(aUniffiMethodName), mUniffiHandle(aUniffiHandle) {}

  // Invoke the callback method using a JS handler
  //
  // For fire-and-forget callbacks, this will return `nullptr`
  MOZ_CAN_RUN_SCRIPT
  virtual already_AddRefed<dom::Promise> MakeCall(
      JSContext* aCx, dom::UniFFICallbackHandler* aJsHandler,
      ErrorResult& aError) = 0;

  // Handle returning a value to Rust.
  //
  // The default implementation does nothing, this is what we use for the `free`
  // callback and also fire-and-forget callbacks.  For async callbacks, we
  // generate a subclass for each return type.
  //
  // HandleReturn will be called on the main thread, and can be invoked
  // synchronously in error cases.
  virtual void HandleReturn(const dom::RootedDictionary<
                                dom::UniFFIScaffoldingCallResult>& aReturnValue,
                            ErrorResult& aError) {}

  virtual ~AsyncCallbackMethodHandlerBase() = default;

  // ---- Generic entry points ----

  // Queue an async call on the JS main thread
  static void ScheduleAsyncCall(
      UniquePtr<AsyncCallbackMethodHandlerBase> aHandler,
      StaticRefPtr<dom::UniFFICallbackHandler>* aJsHandler);

 protected:
  // Name of the callback interface method
  const char* mUniffiMethodName;
  FfiValueInt<uint64_t> mUniffiHandle;

 private:
  // PromiseNativeHandler for async callback interface methods
  //
  // This is appended to the end of the JS promise chain to call the Rust
  // complete function.
  class PromiseHandler final : public dom::PromiseNativeHandler {
   public:
    NS_DECL_ISUPPORTS

    explicit PromiseHandler(UniquePtr<AsyncCallbackMethodHandlerBase> aHandler)
        : mHandler(std::move(aHandler)) {}

    MOZ_CAN_RUN_SCRIPT
    virtual void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                  ErrorResult& aRv) override;
    MOZ_CAN_RUN_SCRIPT
    virtual void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                                  ErrorResult& aRv) override;

   private:
    UniquePtr<AsyncCallbackMethodHandlerBase> mHandler;

    ~PromiseHandler() = default;
  };
};

// Class to handle the free method, this is an implicit method for each callback
// interface. In inputs no arguments and has index=0.
class CallbackFreeHandler : public AsyncCallbackMethodHandlerBase {
 public:
  CallbackFreeHandler(const char* aUniffiMethodName, uint64_t aUniffiHandle)
      : AsyncCallbackMethodHandlerBase(aUniffiMethodName, aUniffiHandle) {}

  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<dom::Promise> MakeCall(
      JSContext* aCx, dom::UniFFICallbackHandler* aJsHandler,
      ErrorResult& aError) override;
};

}  // namespace mozilla::uniffi

#endif  // mozilla_UniFFICallbacks_h
