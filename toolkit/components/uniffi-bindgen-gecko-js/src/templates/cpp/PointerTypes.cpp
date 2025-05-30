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
