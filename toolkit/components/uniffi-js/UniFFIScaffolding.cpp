/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <inttypes.h>
#include "nsError.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/UniFFICall.h"
#include "mozilla/dom/UniFFICallbacks.h"
#include "mozilla/dom/UniFFIScaffolding.h"

// This file implements the UniFFI WebIDL interface by leveraging the generate
// code in UniFFIScaffolding.cpp and UniFFIFixtureScaffolding.cpp.  It's main
// purpose is to check if MOZ_UNIFFI_FIXTURES is set and only try calling the
// scaffolding code if it is.

using mozilla::dom::ArrayBuffer;
using mozilla::dom::GlobalObject;
using mozilla::dom::Promise;
using mozilla::dom::RootedDictionary;
using mozilla::dom::Sequence;
using mozilla::dom::UniFFICallbackHandler;
using mozilla::dom::UniFFIPointer;
using mozilla::dom::UniFFIScaffoldingCallResult;
using mozilla::dom::UniFFIScaffoldingValue;
using mozilla::uniffi::UniffiSyncCallHandler;

namespace mozilla::uniffi {
// Implemented in UniFFIGeneratedScaffolding.cpp
UniquePtr<UniffiSyncCallHandler> GetSyncCallHandler(uint64_t aId);
Maybe<already_AddRefed<UniFFIPointer>> ReadPointer(
    const GlobalObject& aGlobal, uint64_t aId, const ArrayBuffer& aArrayBuff,
    long aPosition, ErrorResult& aError);
bool WritePointer(const GlobalObject& aGlobal, uint64_t aId,
                  const UniFFIPointer& aPtr, const ArrayBuffer& aArrayBuff,
                  long aPosition, ErrorResult& aError);
}  // namespace mozilla::uniffi

namespace mozilla::dom {

// Implement the interface using the generated functions

void UniFFIScaffolding::CallSync(
    const GlobalObject& aGlobal, uint64_t aId,
    const Sequence<UniFFIScaffoldingValue>& aArgs,
    RootedDictionary<UniFFIScaffoldingCallResult>& aReturnValue,
    ErrorResult& aError) {
  if (UniquePtr<UniffiSyncCallHandler> handler =
          uniffi::GetSyncCallHandler(aId)) {
    return UniffiSyncCallHandler::CallSync(std::move(handler), aGlobal, aArgs,
                                           aReturnValue, aError);
  }

  aError.ThrowUnknownError(
      nsPrintfCString("Unknown function id: %" PRIu64, aId));
}

already_AddRefed<Promise> UniFFIScaffolding::CallAsyncWrapper(
    const GlobalObject& aGlobal, uint64_t aId,
    const Sequence<UniFFIScaffoldingValue>& aArgs, ErrorResult& aError) {
  if (UniquePtr<UniffiSyncCallHandler> handler =
          uniffi::GetSyncCallHandler(aId)) {
    return UniffiSyncCallHandler::CallAsyncWrapper(std::move(handler), aGlobal,
                                                   aArgs, aError);
  }

  aError.ThrowUnknownError(
      nsPrintfCString("Unknown function id: %" PRIu64, aId));
  return nullptr;
}

already_AddRefed<UniFFIPointer> UniFFIScaffolding::ReadPointer(
    const GlobalObject& aGlobal, uint64_t aId, const ArrayBuffer& aArrayBuff,
    long aPosition, ErrorResult& aError) {
  Maybe<already_AddRefed<UniFFIPointer>> firstTry =
      uniffi::ReadPointer(aGlobal, aId, aArrayBuff, aPosition, aError);
  if (firstTry.isSome()) {
    return firstTry.extract();
  }

  aError.ThrowUnknownError(nsPrintfCString("Unknown object id: %" PRIu64, aId));
  return nullptr;
}

void UniFFIScaffolding::WritePointer(const GlobalObject& aGlobal, uint64_t aId,
                                     const UniFFIPointer& aPtr,
                                     const ArrayBuffer& aArrayBuff,
                                     long aPosition, ErrorResult& aError) {
  if (uniffi::WritePointer(aGlobal, aId, aPtr, aArrayBuff, aPosition, aError)) {
    return;
  }
  aError.ThrowUnknownError(nsPrintfCString("Unknown object id: %" PRIu64, aId));
}

void UniFFIScaffolding::RegisterCallbackHandler(
    GlobalObject& aGlobal, uint64_t aInterfaceId,
    UniFFICallbackHandler& aCallbackHandler, ErrorResult& aError) {
  uniffi::RegisterCallbackHandler(aInterfaceId, aCallbackHandler, aError);
}

void UniFFIScaffolding::DeregisterCallbackHandler(GlobalObject& aGlobal,
                                                  uint64_t aInterfaceId,
                                                  ErrorResult& aError) {
  uniffi::DeregisterCallbackHandler(aInterfaceId, aError);
}

}  // namespace mozilla::dom
