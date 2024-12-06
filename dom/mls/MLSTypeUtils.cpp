/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MLSBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "nsTArray.h"

namespace mozilla::dom {

nsTArray<uint8_t> ExtractMLSBytesOrUint8ArrayWithUnknownType(
    const MLSBytesOrUint8Array& aArgument, ErrorResult& aRv) {
  // The data can be in a Uint8Array or MLSBytes.
  const Uint8Array* array = nullptr;

  if (aArgument.IsMLSBytes()) {
    array = &aArgument.GetAsMLSBytes().mContent;
  } else {
    MOZ_ASSERT(aArgument.IsUint8Array());
    array = &aArgument.GetAsUint8Array();
  }

  // Append the data from the Uint8Array to the output array
  nsTArray<uint8_t> bytes;
  if (array && !array->AppendDataTo(bytes)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nsTArray<uint8_t>();
  }
  return bytes;
}

nsTArray<uint8_t> ExtractMLSBytesOrUint8Array(
    MLSObjectType aExpectedType, const MLSBytesOrUint8Array& aArgument,
    ErrorResult& aRv) {
  // The data can be in a Uint8Array or MLSBytes.
  const Uint8Array* array = nullptr;

  if (aArgument.IsMLSBytes()) {
    // Check if the type of MLSBytes matches the expected type
    if (aArgument.GetAsMLSBytes().mType != aExpectedType) {
      aRv.ThrowTypeError("Input data has an invalid type");
      return nsTArray<uint8_t>();
    }
    array = &aArgument.GetAsMLSBytes().mContent;
  } else {
    MOZ_ASSERT(aArgument.IsUint8Array());
    array = &aArgument.GetAsUint8Array();
  }

  // Append the data from the Uint8Array to the output array
  nsTArray<uint8_t> bytes;
  if (array && !array->AppendDataTo(bytes)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nsTArray<uint8_t>();
  }
  return bytes;
}

nsTArray<uint8_t> ExtractMLSBytesOrUint8ArrayOrUTF8String(
    MLSObjectType aExpectedType,
    const MLSBytesOrUint8ArrayOrUTF8String& aArgument, ErrorResult& aRv) {
  // The data can be in a Uint8Array, MLSBytes, or UTF8String.
  const Uint8Array* array = nullptr;
  nsTArray<uint8_t> bytes;
  if (aArgument.IsMLSBytes()) {
    // Check if the type of MLSBytes matches the expected type
    if (aArgument.GetAsMLSBytes().mType != aExpectedType) {
      aRv.ThrowTypeError("Input data has an invalid type");
      return nsTArray<uint8_t>();
    }
    array = &aArgument.GetAsMLSBytes().mContent;
  } else if (aArgument.IsUint8Array()) {
    array = &aArgument.GetAsUint8Array();
  } else {
    MOZ_ASSERT(aArgument.IsUTF8String());
    const nsACString& string = aArgument.GetAsUTF8String();
    if (!bytes.AppendElements(string.BeginReading(), string.Length(),
                              fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nsTArray<uint8_t>();
    }
    return bytes;
  }

  // Append the data from the Uint8Array to the output array
  if (array && !array->AppendDataTo(bytes)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nsTArray<uint8_t>();
  }
  return bytes;
}

}  // namespace mozilla::dom
