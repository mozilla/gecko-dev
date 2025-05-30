/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Define scaffolding call classes for each combination of return/argument types
{%- for (preprocessor_condition, scaffolding_calls, preprocessor_condition_end) in scaffolding_calls.iter() %}
{{ preprocessor_condition }}
{%- for scaffolding_call in scaffolding_calls %}
{%- match scaffolding_call.ffi_func.async_data %}
{%- when None %}
class {{ scaffolding_call.handler_class_name }} : public UniffiSyncCallHandler {
private:
  // LowerRustArgs stores the resulting arguments in these fields
  {%- for arg in scaffolding_call.arguments %}
  {{ arg.ffi_value_class }} {{ arg.field_name }}{};
  {%- endfor %}

  // MakeRustCall stores the result of the call in these fields
  {%- if let Some(return_ty) =  scaffolding_call.return_ty %}
  {{ return_ty.ffi_value_class }} mUniffiReturnValue{};
  {%- endif %}

public:
  void LowerRustArgs(const dom::Sequence<dom::OwningUniFFIScaffoldingValue>& aArgs, ErrorResult& aError) override {
    {%- for arg in scaffolding_call.arguments %}
    {{ arg.field_name }}.Lower(aArgs[{{ loop.index0 }}], aError);
    if (aError.Failed()) {
      return;
    }
    {%- endfor %}
  }

  void MakeRustCall(RustCallStatus* aOutStatus) override {
    {%- match scaffolding_call.return_ty %}
    {%- when Some(return_ty) %}
    mUniffiReturnValue = {{ return_ty.ffi_value_class }}::FromRust(
      {{ scaffolding_call.ffi_func.name.0 }}(
        {%- for arg in scaffolding_call.arguments %}
        {{ arg.field_name }}.IntoRust(),
        {%- endfor %}
        aOutStatus
      )
    );
    {%- else %}
    {{ scaffolding_call.ffi_func.name.0 }}(
      {%- for arg in scaffolding_call.arguments %}
      {{ arg.field_name }}.IntoRust(),
      {%- endfor %}
      aOutStatus
    );
    {%- endmatch %}
  }

  virtual void LiftSuccessfulCallResult(JSContext* aCx, dom::Optional<dom::OwningUniFFIScaffoldingValue>& aDest, ErrorResult& aError) override {
    {%- if scaffolding_call.return_ty.is_some() %}
    mUniffiReturnValue.Lift(
      aCx,
      &aDest.Construct(),
      aError
    );
    {%- endif %}
  }
};
{%- when Some(async_data) %}
class {{ scaffolding_call.handler_class_name }} : public UniffiAsyncCallHandler {
public:
  {{ scaffolding_call.handler_class_name }}() : UniffiAsyncCallHandler(
        {{ async_data.ffi_rust_future_poll.0 }},
        {{ async_data.ffi_rust_future_free.0 }}
    ) { }

private:
  // Complete stores the result of the call in mUniffiReturnValue
  {%- if let Some(return_ty) = scaffolding_call.return_ty %}
  {{ return_ty.ffi_value_class }} mUniffiReturnValue{};
  {%- endif %}

protected:
  // Convert a sequence of JS arguments and call the scaffolding function.
  // Always called on the main thread since async Rust calls don't block, they
  // return a future.
  void LowerArgsAndMakeRustCall(const dom::Sequence<dom::OwningUniFFIScaffoldingValue>& aArgs, ErrorResult& aError) override {
    {%- for arg in scaffolding_call.arguments %}
    {{ arg.ffi_value_class }} {{ arg.field_name }}{};
    {{ arg.field_name }}.Lower(aArgs[{{ loop.index0 }}], aError);
    if (aError.Failed()) {
      return;
    }
    {%- endfor %}

    mFutureHandle = {{ scaffolding_call.ffi_func.name.0 }}(
      {%- for arg in scaffolding_call.arguments %}
      {{ arg.field_name }}.IntoRust(){% if !loop.last %},{% endif %}
      {%- endfor %}
    );
  }

  void CallCompleteFn(RustCallStatus* aOutStatus) override {
    {%- match scaffolding_call.return_ty %}
    {%- when Some(return_ty) %}
    mUniffiReturnValue = {{ return_ty.ffi_value_class }}::FromRust(
      {{ async_data.ffi_rust_future_complete.0 }}(mFutureHandle, aOutStatus));
    {%- else %}
    {{ async_data.ffi_rust_future_complete.0 }}(mFutureHandle, aOutStatus);
    {%- endmatch %}
  }

public:
  void LiftSuccessfulCallResult(JSContext* aCx, dom::Optional<dom::OwningUniFFIScaffoldingValue>& aDest, ErrorResult& aError) override {
    {%- if scaffolding_call.return_ty.is_some() %}
    mUniffiReturnValue.Lift(
      aCx,
      &aDest.Construct(),
      aError
    );
    {%- endif %}
  }
};
{%- endmatch %}

{%- endfor %}
{{ preprocessor_condition_end }}
{%- endfor %}

UniquePtr<UniffiSyncCallHandler> GetSyncCallHandler(uint64_t aId) {
  switch (aId) {
    {%- for (preprocessor_condition, scaffolding_calls, preprocessor_condition_end) in scaffolding_calls.iter() %}
{{ preprocessor_condition }}
    {%- for call in scaffolding_calls %}
    {%- if !call.is_async() %}
    case {{ call.id }}: {
      return MakeUnique<{{ call.handler_class_name }}>();
    }
    {%- endif %}
    {%- endfor %}
{{ preprocessor_condition_end }}
    {%- endfor %}

    default:
      return nullptr;
  }
}

UniquePtr<UniffiAsyncCallHandler> GetAsyncCallHandler(uint64_t aId) {
  switch (aId) {
    {%- for (preprocessor_condition, scaffolding_calls, preprocessor_condition_end) in scaffolding_calls.iter() %}
{{ preprocessor_condition }}
    {%- for call in scaffolding_calls %}
    {%- if call.is_async() %}
    case {{ call.id }}: {
      return MakeUnique<{{ call.handler_class_name }}>();
    }
    {%- endif %}
    {%- endfor %}
{{ preprocessor_condition_end }}
    {%- endfor %}

    default:
      return nullptr;
  }
}
