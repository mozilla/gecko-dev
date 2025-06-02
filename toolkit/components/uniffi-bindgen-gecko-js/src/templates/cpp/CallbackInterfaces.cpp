/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */


{%- for (preprocessor_condition, handlers, preprocessor_condition_end) in async_callback_method_handler_bases.iter() %}
{{ preprocessor_condition }}
{%- for handler in handlers %}

class {{ handler.class_name }} : public AsyncCallbackMethodHandlerBase {
public:
  {{ handler.class_name }}(
    const char* aUniffiMethodName,
    uint64_t aUniffiHandle,
    {{ handler.complete_callback_type_name }} aUniffiCompleteCallback,
    uint64_t aUniffiCallbackData
  )
    : AsyncCallbackMethodHandlerBase(aUniffiMethodName, aUniffiHandle),
      mUniffiCompleteCallback(aUniffiCompleteCallback),
      mUniffiCallbackData(aUniffiCallbackData) {}

private:
  {{ handler.complete_callback_type_name }} mUniffiCompleteCallback;
  uint64_t mUniffiCallbackData;

public:
  // Invoke the callback method using a JS handler
  void HandleReturn(const RootedDictionary<UniFFIScaffoldingCallResult>& aCallResult,
                    ErrorResult& aRv) override {
    if (!mUniffiCompleteCallback) {
      MOZ_ASSERT_UNREACHABLE("HandleReturn called multiple times");
      return;
    }

    {{ handler.result_type_name }} result{};
    result.call_status.code = RUST_CALL_INTERNAL_ERROR;
    switch (aCallResult.mCode) {
      case UniFFIScaffoldingCallCode::Success: {
        {% if let Some(return_type) = handler.return_type %}
        if (!aCallResult.mData.WasPassed()) {
          MOZ_LOG(gUniffiLogger, LogLevel::Error, ("[{{ handler.class_name }}] No data passed"));
          break;
        }
        {{ return_type.ffi_value_class }} returnValue;
        returnValue.Lower(aCallResult.mData.Value(), aRv);
        if (aRv.Failed()) {
          MOZ_LOG(gUniffiLogger, LogLevel::Error, ("[{{ handler.class_name }}] Failed to lower return value"));
          break;
        }

        result.return_value = returnValue.IntoRust();
        {% endif %}
        result.call_status.code = RUST_CALL_SUCCESS;
        break;
      }

      case UniFFIScaffoldingCallCode::Error: {
        if (!aCallResult.mData.WasPassed()) {
          MOZ_LOG(gUniffiLogger, LogLevel::Error, ("[{{ handler.class_name }}] No data passed"));
          break;
        }
        FfiValueRustBuffer errorBuf;
        errorBuf.Lower(aCallResult.mData.Value(), aRv);
        if (aRv.Failed()) {
          MOZ_LOG(gUniffiLogger, LogLevel::Error, ("[{{ handler.class_name }}] Failed to lower error buffer"));
          break;
        }

        result.call_status.error_buf = errorBuf.IntoRust();
        result.call_status.code = RUST_CALL_ERROR;
        break;
      }

      default: {
        break;
      }
    }
    mUniffiCompleteCallback(mUniffiCallbackData, result);
    mUniffiCompleteCallback = nullptr;
  }

protected:
  ~{{ handler.class_name }}() {
    if (mUniffiCompleteCallback) {
      MOZ_LOG(gUniffiLogger, LogLevel::Error, ("[{{ handler.class_name }}] promise never completed"));
      {{ handler.result_type_name }} result{};
      result.call_status.code = RUST_CALL_INTERNAL_ERROR;
      mUniffiCompleteCallback(mUniffiCallbackData, result);
    }
  }
};

{%- endfor %}
{{ preprocessor_condition_end }}
{%- endfor %}

// Callback interface method handlers, vtables, etc.
{%- for (preprocessor_condition, callback_interfaces, preprocessor_condition_end) in callback_interfaces.iter() %}
{{ preprocessor_condition }}

{%- for cbi in callback_interfaces %}
static StaticRefPtr<dom::UniFFICallbackHandler> {{ cbi.handler_var }};

{%- for meth in cbi.methods %}
{%- let method_index = loop.index0 %}
{%- let arguments = meth.arguments %}

class {{ meth.handler_class_name }} final : public {{ meth.base_class_name }} {
private:
  // Rust arguments
  {%- for a in arguments %}
  {{ a.ffi_value_class }} {{ a.field_name }}{};
  {%- endfor %}

public:
  {{ meth.handler_class_name }}(
      {%- filter remove_trailing_comma %}
      uint64_t aUniffiHandle,
      {%- for a in arguments %}
      {{ a.ty.type_name }} {{ a.name }},
      {%- endfor %}
      {%- if let Some(async_data) = meth.async_data %}
      {{ async_data.complete_callback_type_name }} aUniffiCompleteCallback,
      uint64_t aUniffiCallbackData,
      {%- endif %}
      {%- endfilter %})
    : {{ meth.base_class_name }}(
        {%- filter remove_trailing_comma %}
        "{{ cbi.name }}.{{ meth.fn_name }}",
        aUniffiHandle,
        {%- if meth.is_async() %}
        aUniffiCompleteCallback,
        aUniffiCallbackData
        {%- endif %}
        {%- endfilter %}
    )
    {%- for a in arguments %}, {{ a.field_name }}({{ a.ffi_value_class }}::FromRust({{ a.name }})){% endfor %}
  {
  }

  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<dom::Promise>
  MakeCall(JSContext* aCx, dom::UniFFICallbackHandler* aJsHandler, ErrorResult& aError) override {
    nsTArray<dom::OwningUniFFIScaffoldingValue> uniffiArgs;

    // Setup
    if (!uniffiArgs.AppendElements({{ arguments.len()  }}, mozilla::fallible)) {
      aError.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    // Convert each argument
    {%- for a in arguments %}
    {{ a.field_name }}.Lift(
      aCx,
      &uniffiArgs[{{ loop.index0 }}],
      aError);
    if (aError.Failed()) {
      return nullptr;
    }
    {%- endfor %}

    RefPtr<dom::Promise> result = aJsHandler->CallAsync(mUniffiHandle.IntoRust(), {{ method_index }}, uniffiArgs, aError);
    {%- if meth.is_async() %}
    return result.forget();
    {%- else %}
    {# Return `nullptr` for fire-and-forget callbacks, to avoid registering a promise result listener #}
    return nullptr;
    {%- endif %}
  }
};

{% match meth.async_data -%}
{% when None %}
// Sync callback methods are always wrapped to be fire-and-forget style async callbacks.  This means
// we schedule the callback asynchronously and ignore the return value and any exceptions thrown.
extern "C" void {{ meth.fn_name }}(
  uint64_t aUniffiHandle,
  {%- for a in meth.arguments %}
  {{ a.ty.type_name }} {{ a.name }},
  {%- endfor %}
  {{ meth.out_pointer_ty.type_name }} aUniffiOutReturn,
  RustCallStatus* uniffiOutStatus
) {
  UniquePtr<AsyncCallbackMethodHandlerBase> handler = MakeUnique<{{ meth.handler_class_name }}>(aUniffiHandle{% for a in arguments %}, {{ a.name }}{%- endfor %});
  AsyncCallbackMethodHandlerBase::ScheduleAsyncCall(std::move(handler), &{{ cbi.handler_var }});
}
{% when Some(async_data) -%}
extern "C" void {{ meth.fn_name }}(
  uint64_t aUniffiHandle,
  {%- for a in meth.arguments %}
  {{ a.ty.type_name }} {{ a.name }},
  {%- endfor %}
  {{ async_data.complete_callback_type_name }} aUniffiForeignFutureCallback,
  uint64_t aUniffiForeignFutureCallbackData,
  // This can be used to detected when the future is dropped from the Rust side and cancel the
  // async task on the foreign side.  However, there's no way to do that in JS, so we just ignore
  // it.
  ForeignFuture *aUniffiOutForeignFuture
) {
  UniquePtr<AsyncCallbackMethodHandlerBase> handler = MakeUnique<{{ meth.handler_class_name }}>(
        aUniffiHandle,
        {% for a in arguments -%}
        {{ a.name }},
        {% endfor -%}
        aUniffiForeignFutureCallback,
        aUniffiForeignFutureCallbackData);
  // Now that everything is set up, schedule the call in the JS main thread.
  AsyncCallbackMethodHandlerBase::ScheduleAsyncCall(std::move(handler), &{{ cbi.handler_var }});
}
{%- endmatch %}

{%- endfor %}

extern "C" void {{ cbi.free_fn }}(uint64_t uniffiHandle) {
   // Callback object handles are keys in a map stored in the JS handler. To
   // handle the free call, schedule a fire-and-forget JS call to remove the key.
   AsyncCallbackMethodHandlerBase::ScheduleAsyncCall(
      MakeUnique<CallbackFreeHandler>("{{ cbi.name }}.uniffi_free", uniffiHandle),
      &{{ cbi.handler_var }});
}

static {{ cbi.vtable_struct_type.type_name }} {{ cbi.vtable_var }} {
  {%- for meth in cbi.methods %}
  {{ meth.fn_name }},
  {%- endfor %}
  {{ cbi.free_fn }}
};

{%- endfor %}
{{ preprocessor_condition_end }}
{%- endfor %}

void RegisterCallbackHandler(uint64_t aInterfaceId, UniFFICallbackHandler& aCallbackHandler, ErrorResult& aError) {
  switch (aInterfaceId) {
    {%- for (preprocessor_condition, callback_interfaces, preprocessor_condition_end) in callback_interfaces.iter() %}
    {{ preprocessor_condition }}

    {%- for cbi in callback_interfaces %}
    case {{ cbi.id }}: {
      if ({{ cbi.handler_var }}) {
        aError.ThrowUnknownError("[UniFFI] Callback handler already registered for {{ cbi.name }}"_ns);
        return;
      }

      {{ cbi.handler_var }} = &aCallbackHandler;
      {{ cbi.init_fn.0 }}(&{{ cbi.vtable_var }});
      break;
    }


    {%- endfor %}
    {{ preprocessor_condition_end }}
    {%- endfor %}

    default:
      aError.ThrowUnknownError(nsPrintfCString("RegisterCallbackHandler: Unknown callback interface id (%" PRIu64 ")", aInterfaceId));
      return;
  }
}

void DeregisterCallbackHandler(uint64_t aInterfaceId, ErrorResult& aError) {
  switch (aInterfaceId) {
    {%- for (preprocessor_condition, callback_interfaces, preprocessor_condition_end) in callback_interfaces.iter() %}
    {{ preprocessor_condition }}

    {%- for cbi in callback_interfaces %}
    case {{ cbi.id }}: {
      if (!{{ cbi.handler_var }}) {
        aError.ThrowUnknownError("[UniFFI] Callback handler not registered for {{ cbi.name }}"_ns);
        return;
      }

      {{ cbi.handler_var }} = nullptr;
      break;
    }


    {%- endfor %}
    {{ preprocessor_condition_end }}
    {%- endfor %}

    default:
      aError.ThrowUnknownError(nsPrintfCString("DeregisterCallbackHandler: Unknown callback interface id (%" PRIu64 ")", aInterfaceId));
      return;
  }
}
