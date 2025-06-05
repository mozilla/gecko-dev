/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Define pointer types
{%- for (preprocessor_condition, pointer_types, preprocessor_condition_end) in pointer_types.iter() %}
{{ preprocessor_condition }}
{%- for pointer_type in pointer_types %}
const static mozilla::uniffi::UniFFIPointerType {{ pointer_type.name }} {
  "{{ pointer_type.label }}"_ns,
  {{ pointer_type.ffi_func_clone.0 }},
  {{ pointer_type.ffi_func_free.0 }},
};

{%- match pointer_type.trait_interface_info %}
{%- when None %}
class {{ pointer_type.ffi_value_class }} {
 private:
  void* mValue = nullptr;

 public:
  {{ pointer_type.ffi_value_class }}() = default;
  explicit {{ pointer_type.ffi_value_class }}(void* aValue) : mValue(aValue) {}

  // Delete copy constructor and assignment as this type is non-copyable.
  {{ pointer_type.ffi_value_class }}(const {{ pointer_type.ffi_value_class }}&) = delete;
  {{ pointer_type.ffi_value_class }}& operator=(const {{ pointer_type.ffi_value_class }}&) = delete;

  {{ pointer_type.ffi_value_class }}& operator=({{ pointer_type.ffi_value_class }}&& aOther) {
    FreeHandle();
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
    if (!value.IsSamePtrType(&{{ pointer_type.name }})) {
      aError.ThrowTypeError("Incorrect UniFFI pointer type"_ns);
      return;
    }
    FreeHandle();
    mValue = value.ClonePtr();
  }

  // LowerReceiver is used for method receivers.  For non-trait interfaces, it works exactly the
  // same as `Lower`
  void LowerReciever(const dom::OwningUniFFIScaffoldingValue& aValue,
             ErrorResult& aError) {
    Lower(aValue, aError);
  }

  void Lift(JSContext* aContext, dom::OwningUniFFIScaffoldingValue* aDest,
            ErrorResult& aError) {
    aDest->SetAsUniFFIPointer() =
        dom::UniFFIPointer::Create(mValue, &{{ pointer_type.name }});
    mValue = nullptr;
  }

  void* IntoRust() {
    auto temp = mValue;
    mValue = nullptr;
    return temp;
  }

  static {{ pointer_type.ffi_value_class }} FromRust(void* aValue) {
    return {{ pointer_type.ffi_value_class }}(aValue);
  }

  void FreeHandle() {
    if (mValue) {
      RustCallStatus callStatus{};
      ({{ pointer_type.ffi_func_free.0 }})(mValue, &callStatus);
      // No need to check `RustCallStatus`, it's only part of the API to match
      // other FFI calls.  The free function can never fail.
    }
  }

  ~{{ pointer_type.ffi_value_class }}() {
    // If the pointer is non-null, this means Lift/IntoRust was never called
    // because there was some failure along the way. Free the pointer to avoid a
    // leak
    FreeHandle();
  }
};
{%- when Some(trait_interface_info) %}
// Forward declare the free function, which is defined later on in `CallbackInterfaces.cpp`
extern "C" void {{ trait_interface_info.free_fn }}(uint64_t uniffiHandle);

// Trait interface FFI value class.  This is a hybrid between the one for interfaces and callback
// interface version
class {{ pointer_type.ffi_value_class }} {
 private:
  // Did we lower a callback interface, rather than lift an object interface?
  // This is weird, but it's a needed work until something like
  // https://github.com/mozilla/uniffi-rs/pull/1823 lands.
  bool mLoweredCallbackInterface = false;
  // The raw FFI value is a pointer.
  // For callback interfaces, the uint64_t handle gets casted to a pointer.  Callback interface
  // handles are incremented by one at a time, so even on a 32-bit system this
  // shouldn't overflow.
  void* mValue = nullptr;

 public:
  {{ pointer_type.ffi_value_class }}() = default;
  explicit {{ pointer_type.ffi_value_class }}(void* aValue) : mValue(aValue) {}

  // Delete copy constructor and assignment as this type is non-copyable.
  {{ pointer_type.ffi_value_class }}(const {{ pointer_type.ffi_value_class }}&) = delete;
  {{ pointer_type.ffi_value_class }}& operator=(const {{ pointer_type.ffi_value_class }}&) = delete;

  {{ pointer_type.ffi_value_class }}& operator=({{ pointer_type.ffi_value_class }}&& aOther) {
    FreeHandle();
    mValue = aOther.mValue;
    mLoweredCallbackInterface = aOther.mLoweredCallbackInterface;
    aOther.mValue = nullptr;
    aOther.mLoweredCallbackInterface = false;
    return *this;
  }

  // Lower treats `aValue` as a callback interface
  void Lower(const dom::OwningUniFFIScaffoldingValue& aValue,
             ErrorResult& aError) {
    if (!aValue.IsDouble()) {
      aError.ThrowTypeError("Bad argument type"_ns);
      return;
    }
    double floatValue = aValue.GetAsDouble();
    uint64_t intValue = static_cast<uint64_t>(floatValue);
    if (intValue != floatValue) {
      aError.ThrowTypeError("Not an integer"_ns);
      return;
    }
    FreeHandle();
    mValue = reinterpret_cast<void *>(intValue);
    mLoweredCallbackInterface = true;
  }

  // LowerReceiver is used for method receivers.  It treats `aValue` as an object pointer.
  void LowerReciever(const dom::OwningUniFFIScaffoldingValue& aValue,
             ErrorResult& aError) {
    if (!aValue.IsUniFFIPointer()) {
      aError.ThrowTypeError("Expected UniFFI pointer argument"_ns);
      return;
    }
    dom::UniFFIPointer& value = aValue.GetAsUniFFIPointer();
    if (!value.IsSamePtrType(&{{ pointer_type.name }})) {
      aError.ThrowTypeError("Incorrect UniFFI pointer type"_ns);
      return;
    }
    FreeHandle();
    mValue = value.ClonePtr();
    mLoweredCallbackInterface = false;
  }

  // Lift treats `aDest` as a regular interface
  void Lift(JSContext* aContext, dom::OwningUniFFIScaffoldingValue* aDest,
            ErrorResult& aError) {
    aDest->SetAsUniFFIPointer() =
        dom::UniFFIPointer::Create(mValue, &{{ pointer_type.name }});
    mValue = nullptr;
    mLoweredCallbackInterface = false;
  }

  void* IntoRust() {
    auto temp = mValue;
    mValue = nullptr;
    mLoweredCallbackInterface = false;
    return temp;
  }

  static {{ pointer_type.ffi_value_class }} FromRust(void* aValue) {
    return {{ pointer_type.ffi_value_class }}(aValue);
  }

  void FreeHandle() {
    // This behavior depends on if we lowered a callback interface handle or lifted an interface
    // pointer.
    if (mLoweredCallbackInterface && reinterpret_cast<uintptr_t>(mValue) != 0) {
                                     printf("FREEING CB %p\n", mValue);
        {{ trait_interface_info.free_fn }}(reinterpret_cast<uintptr_t>(mValue));
        mValue = reinterpret_cast<void *>(0);
    } else if (!mLoweredCallbackInterface && mValue != nullptr) {
                                     printf("FREEING interface %p\n", mValue);
      RustCallStatus callStatus{};
      ({{ pointer_type.ffi_func_free.0 }})(mValue, &callStatus);
      // No need to check `RustCallStatus`, it's only part of the API to match
      // other FFI calls.  The free function can never fail.
    }
    mValue = nullptr;
    mLoweredCallbackInterface = false;
  }

  ~{{ pointer_type.ffi_value_class }}() {
    // If the pointer is non-null, this means Lift/IntoRust was never called
    // because there was some failure along the way. Free the pointer to avoid a
    // leak
    FreeHandle();
  }
};
{%- endmatch %}

{%- endfor %}
{{ preprocessor_condition_end }}
{%- endfor %}

Maybe<already_AddRefed<UniFFIPointer>> ReadPointer(const GlobalObject& aGlobal, uint64_t aId, const ArrayBuffer& aArrayBuff, long aPosition, ErrorResult& aError) {
  const UniFFIPointerType* type;
  switch (aId) {
    {%- for (preprocessor_condition, pointer_types, preprocessor_condition_end) in pointer_types.iter() %}
{{ preprocessor_condition }}
    {%- for pointer_type in pointer_types %}
    case {{ pointer_type.id }}: {
      type = &{{ pointer_type.name }};
      break;
    }
    {%- endfor %}
{{ preprocessor_condition_end }}
    {%- endfor %}
    default:
      return Nothing();
  }
  return Some(UniFFIPointer::Read(aArrayBuff, aPosition, type, aError));
}

bool WritePointer(const GlobalObject& aGlobal, uint64_t aId, const UniFFIPointer& aPtr, const ArrayBuffer& aArrayBuff, long aPosition, ErrorResult& aError) {
  const UniFFIPointerType* type;
  switch (aId) {
    {%- for (preprocessor_condition, pointer_types, preprocessor_condition_end) in pointer_types.iter() %}
{{ preprocessor_condition }}
    {%- for pointer_type in pointer_types %}
    case {{ pointer_type.id }}: {
      type = &{{ pointer_type.name }};
      break;
    }
    {%- endfor %}
{{ preprocessor_condition_end }}
    {%- endfor %}
    default:
      return false;
  }
  aPtr.Write(aArrayBuff, aPosition, type, aError);
  return true;
}
