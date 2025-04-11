/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UniFFIFfiConverter_h
#define mozilla_UniFFIFfiConverter_h

#include <limits>
#include <type_traits>
#include "nsString.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/PrimitiveConversions.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/UniFFIBinding.h"
#include "mozilla/dom/UniFFIPointer.h"
#include "mozilla/dom/UniFFIScaffolding.h"
#include "mozilla/uniffi/OwnedRustBuffer.h"
#include "mozilla/uniffi/PointerType.h"
#include "mozilla/uniffi/Rust.h"

namespace mozilla::uniffi {

// This header defines the `FfiValue*` classes, which handle conversions between
// FFI values and the JS `OwningUniFFIScaffoldingValue` class.
//
// The exact signatures vary slightly, but in all FfiValue classes define these
// functions:
// - `Lower` -- Convert a `OwningUniFFIScaffoldingValue` into an `FfiValue`.
// - `Lift` -- Convert a `FfiValue` into a `OwningUniFFIScaffoldingValue`.
// - `IntoRust` -- Convert a `FfiValue` into a raw FFI type to pass to Rust
// - `FromRust` -- Convert a raw FFI type from Rust into a `FfiValue`
//
// Also, each `FfiValue` class defines a default constructor.
// For types that hold resources like `FfiValueRustBuffer`, `Lift` and
// `IntoRust` move resources out of the value, leaving behind the default.

// FfiValue class for integer values
template <typename T>
class FfiValueInt {
 private:
  T mValue = 0;

 public:
  FfiValueInt() = default;
  explicit FfiValueInt(T aValue) : mValue(aValue) {}

  void Lower(const dom::OwningUniFFIScaffoldingValue& aValue,
             ErrorResult& aError) {
    if (!aValue.IsDouble()) {
      aError.ThrowTypeError("Bad argument type"_ns);
      return;
    }
    double floatValue = aValue.GetAsDouble();

    // Use PrimitiveConversionTraits_Limits rather than std::numeric_limits,
    // since it handles JS-specific bounds like the 64-bit integer limits.
    // (see Number.MAX_SAFE_INTEGER and Number.MIN_SAFE_INTEGER)
    if (floatValue < dom::PrimitiveConversionTraits_Limits<T>::min() ||
        floatValue > dom::PrimitiveConversionTraits_Limits<T>::max()) {
      aError.ThrowRangeError("Integer value is out of range"_ns);
      return;
    }

    T intValue = static_cast<T>(floatValue);
    if (intValue != floatValue) {
      aError.ThrowTypeError("Not an integer"_ns);
      return;
    }
    mValue = intValue;
  }

  void Lift(JSContext* aContext, dom::OwningUniFFIScaffoldingValue* aDest,
            ErrorResult& aError) {
    if (mValue < dom::PrimitiveConversionTraits_Limits<T>::min() ||
        mValue > dom::PrimitiveConversionTraits_Limits<T>::max()) {
      aError.ThrowRangeError(
          "64-bit value cannot be precisely represented in JS"_ns);
      return;
    }
    aDest->SetAsDouble() = mValue;
  }

  T IntoRust() { return mValue; }

  static FfiValueInt FromRust(T aValue) { return FfiValueInt(aValue); };
};

// FfiValue class for floating point values
template <typename T>
class FfiValueFloat {
 private:
  T mValue = 0.0;

 public:
  FfiValueFloat() = default;
  explicit FfiValueFloat(T aValue) : mValue(aValue) {}

  void Lower(const dom::OwningUniFFIScaffoldingValue& aValue,
             ErrorResult& aError) {
    if (!aValue.IsDouble()) {
      aError.ThrowTypeError("Bad argument type"_ns);
      return;
    }
    mValue = aValue.GetAsDouble();
  }

  void Lift(JSContext* aContext, dom::OwningUniFFIScaffoldingValue* aDest,
            ErrorResult& aError) {
    aDest->SetAsDouble() = mValue;
  }

  T IntoRust() { return mValue; }

  static FfiValueFloat FromRust(T aValue) { return FfiValueFloat(aValue); }
};

class FfiValueRustBuffer {
 private:
  OwnedRustBuffer mValue;

 public:
  FfiValueRustBuffer() = default;
  explicit FfiValueRustBuffer(RustBuffer aValue)
      : mValue(OwnedRustBuffer(aValue)) {}
  explicit FfiValueRustBuffer(OwnedRustBuffer aValue)
      : mValue(std::move(aValue)) {}

  void Lower(const dom::OwningUniFFIScaffoldingValue& aValue,
             ErrorResult& aError) {
    if (!aValue.IsArrayBuffer()) {
      aError.ThrowTypeError("Expected ArrayBuffer argument"_ns);
      return;
    }
    mValue = OwnedRustBuffer::FromArrayBuffer(aValue.GetAsArrayBuffer());
  }

  void Lift(JSContext* aContext, dom::OwningUniFFIScaffoldingValue* aDest,
            ErrorResult& aError) {
    JS::Rooted<JSObject*> obj(aContext);
    mValue.IntoArrayBuffer(aContext, &obj, aError);
    if (aError.Failed()) {
      return;
    }
    aDest->SetAsArrayBuffer().Init(obj);
  }

  RustBuffer IntoRust() { return mValue.IntoRustBuffer(); }

  static FfiValueRustBuffer FromRust(RustBuffer aValue) {
    return FfiValueRustBuffer(OwnedRustBuffer(aValue));
  }

  bool IsSet() { return mValue.IsValid(); }
};

template <const UniFFIPointerType* PointerType>
class FfiValueObjectHandle {
 private:
  void* mValue = nullptr;

 public:
  FfiValueObjectHandle() = default;
  explicit FfiValueObjectHandle(void* aValue) : mValue(aValue) {}

  // Delete copy constructor and assignment as this type is non-copyable.
  FfiValueObjectHandle(const FfiValueObjectHandle&) = delete;
  FfiValueObjectHandle& operator=(const FfiValueObjectHandle&) = delete;

  FfiValueObjectHandle& operator=(FfiValueObjectHandle&& aOther) {
    if (mValue && mValue != aOther.mValue) {
      FreeHandle();
    }
    mValue = aOther.mValue;
    aOther.mValue = nullptr;
    return *this;
  }

  void Lower(const dom::OwningUniFFIScaffoldingValue& aValue,
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
    FreeHandle();
    mValue = value.ClonePtr();
  }

  void Lift(JSContext* aContext, dom::OwningUniFFIScaffoldingValue* aDest,
            ErrorResult& aError) {
    aDest->SetAsUniFFIPointer() =
        dom::UniFFIPointer::Create(mValue, PointerType);
    mValue = nullptr;
  }

  void* IntoRust() {
    auto temp = mValue;
    mValue = nullptr;
    return temp;
  }

  static FfiValueObjectHandle FromRust(void* aValue) {
    return FfiValueObjectHandle(aValue);
  }

  void FreeHandle() {
    if (mValue) {
      RustCallStatus callStatus{};
      (PointerType->destructor)(mValue, &callStatus);
      // No need to check `RustCallStatus`, it's only part of the API to match
      // other FFI calls.  The free function can never fail.
    }
  }

  ~FfiValueObjectHandle() {
    // If the pointer is non-null, this means Lift/IntoRust was never called
    // because there was some failure along the way. Free the pointer to avoid a
    // leak
    FreeHandle();
  }
};

}  // namespace mozilla::uniffi

#endif  // mozilla_UniFFIFfiConverter_h
