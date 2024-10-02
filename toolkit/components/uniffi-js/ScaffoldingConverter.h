/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScaffoldingConverter_h
#define mozilla_ScaffoldingConverter_h

#include <limits>
#include <type_traits>
#include "nsString.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/OwnedRustBuffer.h"
#include "mozilla/dom/PrimitiveConversions.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/UniFFIBinding.h"
#include "mozilla/dom/UniFFIPointer.h"
#include "mozilla/dom/UniFFIPointerType.h"
#include "mozilla/dom/UniFFIRust.h"
#include "mozilla/dom/UniFFIScaffolding.h"

namespace mozilla::uniffi {

// Handle converting types between JS and Rust
//
// Scaffolding conversions are done using a 2 step process:
//   - Call FromJs/FromRust to convert to an intermediate type
//   - Call IntoJs/IntoRust to convert from that type to the target type
//
// The main reason for this is handling RustBuffers when other arguments fail
// to convert.  By using OwnedRustBuffer as the intermediate type, we can
// ensure those buffers get freed in that case.  Note that we can't use
// OwnedRustBuffer as the Rust type.  Passing the buffer into Rust transfers
// ownership so we shouldn't free the buffer in this case.
//
// For most other types, we just use the Rust type as the intermediate type.
template <typename T>
class ScaffoldingConverter {
 public:
  using RustType = T;
  using IntermediateType = T;

  // Convert a JS value to an intermedate type
  //
  // This inputs a const ref, because that's what the WebIDL bindings send to
  // us.
  //
  // If this succeeds then IntoRust is also guaranteed to succeed
  static void FromJs(const dom::UniFFIScaffoldingValue& aValue,
                     IntermediateType* aResult, ErrorResult& aError) {
    if (!aValue.IsDouble()) {
      aError.ThrowTypeError("Bad argument type"_ns);
      return;
    }
    double value = aValue.GetAsDouble();

    if (std::isnan(value)) {
      aError.ThrowUnknownError("NaN not allowed"_ns);
      return;
    }

    if constexpr (std::is_integral<RustType>::value) {
      // Use PrimitiveConversionTraits_Limits rather than std::numeric_limits,
      // since it handles JS-specific bounds like the 64-bit integer limits.
      // (see Number.MAX_SAFE_INTEGER and Number.MIN_SAFE_INTEGER)
      if (value < dom::PrimitiveConversionTraits_Limits<RustType>::min() ||
          value > dom::PrimitiveConversionTraits_Limits<RustType>::max()) {
        aError.ThrowRangeError(
            "UniFFI return value cannot be precisely represented in JS"_ns);
        return;
      }
    }

    // Don't check float bounds for a few reasons.
    //   - It's difficult because
    //     PrimitiveConversionTraits_Limits<float>::min() is the smallest
    //     positive value, rather than the most negative.
    //   - A float value unlikely to overflow
    //   - It's also likely that we can't do an exact conversion because the
    //     float doesn't have enough precision, but it doesn't seem correct
    //     to error out in that case.

    IntermediateType rv = static_cast<IntermediateType>(value);
    if constexpr (std::is_integral<IntermediateType>::value) {
      if (rv != value) {
        aError.ThrowTypeError("Not an integer"_ns);
        return;
      }
    }
    *aResult = rv;
  }

  // Convert an intermediate type to a Rust type
  //
  // IntoRust doesn't touch the JS data, so it's safe to call in a worker thread
  static RustType IntoRust(IntermediateType aValue) { return aValue; }

  // Convert an Rust type to an intermediate type
  //
  // This inputs a value since Rust types are POD types
  static IntermediateType FromRust(RustType aValue) { return aValue; }

  // Convert an intermedate type to a JS type
  //
  // This inputs an r-value reference since we may want to move data out of
  // this type.
  static void IntoJs(JSContext* aContext, IntermediateType&& aValue,
                     dom::UniFFIScaffoldingValue* aDest, ErrorResult& aError) {
    if constexpr (std::is_same<RustType, int64_t>::value ||
                  std::is_same<RustType, uint64_t>::value) {
      // Check that the value can fit in a double (only needed for 64 bit types)
      if (aValue < dom::PrimitiveConversionTraits_Limits<RustType>::min() ||
          aValue > dom::PrimitiveConversionTraits_Limits<RustType>::max()) {
        aError.ThrowRangeError(
            "UniFFI return value cannot be precisely represented in JS"_ns);
        return;
      }
    }
    if constexpr (std::is_floating_point<RustType>::value) {
      if (std::isnan(aValue)) {
        aError.ThrowUnknownError("NaN not allowed"_ns);
        return;
      }
    }
    aDest->SetAsDouble() = aValue;
  }
};

template <>
class ScaffoldingConverter<RustBuffer> {
 public:
  using RustType = RustBuffer;
  using IntermediateType = OwnedRustBuffer;

  static void FromJs(const dom::UniFFIScaffoldingValue& aValue,
                     OwnedRustBuffer* aResult, ErrorResult& aError) {
    if (!aValue.IsArrayBuffer()) {
      aError.ThrowTypeError("Expected ArrayBuffer argument"_ns);
      return;
    }
    *aResult = OwnedRustBuffer::FromArrayBuffer(aValue.GetAsArrayBuffer());
  }

  static RustBuffer IntoRust(OwnedRustBuffer&& aValue) {
    return aValue.IntoRustBuffer();
  }

  static OwnedRustBuffer FromRust(RustBuffer aValue) {
    return OwnedRustBuffer(aValue);
  }

  static void IntoJs(JSContext* aContext, OwnedRustBuffer&& aValue,
                     dom::UniFFIScaffoldingValue* aDest, ErrorResult& aError) {
    JS::Rooted<JSObject*> obj(aContext);
    aValue.IntoArrayBuffer(aContext, &obj, aError);
    if (aError.Failed()) {
      return;
    }
    aDest->SetAsArrayBuffer().Init(obj);
  }
};

// ScaffoldingConverter for object pointers
template <const UniFFIPointerType* PointerType>
class ScaffoldingObjectConverter {
 public:
  using RustType = void*;
  using IntermediateType = void*;

  static void FromJs(const dom::UniFFIScaffoldingValue& aValue, void** aResult,
                     ErrorResult& aError) {
    if (!aValue.IsUniFFIPointer()) {
      aError.ThrowTypeError("Expected UniFFI pointer argument"_ns);
      return;
    }
    dom::UniFFIPointer& value = aValue.GetAsUniFFIPointer();
    if (!value.IsSamePtrType(PointerType)) {
      aError.ThrowTypeError("Incorrect UniFFI pointer type"_ns);
      return;
    }
    *aResult = value.ClonePtr();
  }

  static void* IntoRust(void* aValue) { return aValue; }

  static void* FromRust(void* aValue) { return aValue; }

  static void IntoJs(JSContext* aContext, void* aValue,
                     dom::UniFFIScaffoldingValue* aDest, ErrorResult& aError) {
    aDest->SetAsUniFFIPointer() =
        dom::UniFFIPointer::Create(aValue, PointerType);
  }
};

// ScaffoldingConverter for void returns
//
// This doesn't implement the normal interface, it's only use is a the
// ReturnConverter parameter of ScaffoldingCallHandler.
template <>
class ScaffoldingConverter<void> {
 public:
  using RustType = void;
};

}  // namespace mozilla::uniffi

#endif  // mozilla_ScaffoldingConverter_h
