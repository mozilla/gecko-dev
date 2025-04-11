/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UniFFICallbacks_h
#define mozilla_UniFFICallbacks_h

#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/UniFFIRust.h"
#include "mozilla/dom/UniFFIScaffolding.h"

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
 * Implemented by generated code for each callback interface.
 *
 * The generated subclass handles the specifics of each call, while the code in
 * the base class handles generic aspects of the call
 *
 * The generated subclass stores all data needed to make the call, including the
 * arguments passed from Rust internally. MakeCall must only be called
 * once-per-object, since it may consume some of the arguments. This means that
 * we create a new UniffiCallbackMethodHandlerBase subclass instance for each
 * callback interface call from Rust.
 */
class UniffiCallbackMethodHandlerBase {
 protected:
  // Name of the callback interface
  const char* mInterfaceName;
  uint64_t mObjectHandle;

  // Invoke the callback method using a JS handler
  MOZ_CAN_RUN_SCRIPT
  virtual void MakeCall(JSContext* aCx, dom::UniFFICallbackHandler* aJsHandler,
                        ErrorResult& aError) = 0;

 public:
  UniffiCallbackMethodHandlerBase(const char* aInterfaceName,
                                  uint64_t aObjectHandle)
      : mInterfaceName(aInterfaceName), mObjectHandle(aObjectHandle) {}

  virtual ~UniffiCallbackMethodHandlerBase() = default;

  // ---- Generic entry points ----

  // Queue the method to be called asynchronously and ignore the return value.
  //
  // This is for fire-and-forget callbacks where the caller doesn't care about
  // the return value and doesn't want to wait for the call to finish.  A good
  // use case for this is logging.
  //
  // FireAndForget is responsible for checking that the aJsHandler is non-null,
  // this way we don't need to duplicate the null check in the generated code.
  static void FireAndForget(
      UniquePtr<UniffiCallbackMethodHandlerBase> aHandler,
      StaticRefPtr<dom::UniFFICallbackHandler>* aJsHandler);
};

// Class to handle the free method, this is an implicit method for each callback
// interface. In inputs no arguments and has index=0.
class UniffiCallbackFreeHandler : public UniffiCallbackMethodHandlerBase {
 public:
  UniffiCallbackFreeHandler(const char* aInterfaceName, uint64_t aObjectHandle)
      : UniffiCallbackMethodHandlerBase(aInterfaceName, aObjectHandle) {}
  void MakeCall(JSContext* aCx, dom::UniFFICallbackHandler* aJsHandler,
                ErrorResult& aError) override;
};

}  // namespace mozilla::uniffi

#endif  // mozilla_UniFFICallbacks_h
