/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Callback interface method handlers, vtables, etc.
{%- for (preprocessor_condition, callback_interfaces, preprocessor_condition_end) in callback_interfaces.iter() %}
{{ preprocessor_condition }}

{%- for cbi in callback_interfaces %}
static StaticRefPtr<dom::UniFFICallbackHandler> {{ cbi.handler_var }};

{%- for meth in cbi.methods %}
{%- let method_index = loop.index0 %}
{%- let arguments = meth.arguments %}

class {{ meth.handler_class_name }} : public UniffiCallbackMethodHandlerBase {
private:
  // Rust arguments
  {%- for a in arguments %}
  {{ a.ffi_value_class }} {{ a.field_name }}{};
  {%- endfor %}

public:
  {{ meth.handler_class_name }}(uint64_t aUniffiHandle{%- for a in arguments %}, {{ a.ty.type_name }} {{ a.name }}{%- endfor %})
    : UniffiCallbackMethodHandlerBase("{{ cbi.name }}", aUniffiHandle)
    {%- for a in arguments %}, {{ a.field_name }}({{ a.ffi_value_class }}::FromRust({{ a.name }})){% endfor %} {
  }

  MOZ_CAN_RUN_SCRIPT
  void MakeCall(JSContext* aCx, dom::UniFFICallbackHandler* aJsHandler, ErrorResult& aError) override {
    nsTArray<dom::OwningUniFFIScaffoldingValue> uniffiArgs;

    // Setup
    if (!uniffiArgs.AppendElements({{ arguments.len()  }}, mozilla::fallible)) {
      aError.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    // Convert each argument
    {%- for a in arguments %}
    {{ a.field_name }}.Lift(
      aCx,
      &uniffiArgs[{{ loop.index0 }}],
      aError);
    if (aError.Failed()) {
        return;
    }
    {%- endfor %}

    // Stores the return value.  For now, we currently don't do anything with it, since we only support
    // fire-and-forget callbacks.
    NullableRootedUnion<dom::OwningUniFFIScaffoldingValue> returnValue(aCx);
    // Make the call
    aJsHandler->Call(mUniffiHandle.IntoRust(), {{ method_index }}, uniffiArgs, returnValue, aError);
  }
};

extern "C" void {{ meth.fn_name }}(
    uint64_t aUniffiHandle,
    {%- for a in meth.arguments %}
    {{ a.ty.type_name }} {{ a.name }},
    {%- endfor %}
    {{ meth.out_pointer_ty.type_name }} aUniffiOutReturn,
    RustCallStatus* uniffiOutStatus
) {
  UniquePtr<UniffiCallbackMethodHandlerBase> handler = MakeUnique<{{ meth.handler_class_name }}>(aUniffiHandle{% for a in arguments %}, {{ a.name }}{%- endfor %});
  // Note: currently we only support queueing fire-and-forget async callbacks

  // For fire-and-forget callbacks, we don't know if the method succeeds or not
  // since it's called later. uniffiCallStatus is initialized to a successful
  // state by the Rust code, so there's no need to modify it.
  UniffiCallbackMethodHandlerBase::FireAndForget(std::move(handler), &{{ cbi.handler_var }});
}

{%- endfor %}

extern "C" void {{ cbi.free_fn }}(uint64_t uniffiHandle) {
  // Callback object handles are keys in a map stored in the JS handler. To
  // handle the free call, make a call into JS which will remove the key.
  // Fire-and-forget is perfect for this.
  UniffiCallbackMethodHandlerBase::FireAndForget(MakeUnique<UniffiCallbackFreeHandler>("{{ cbi.name }}", uniffiHandle), &{{ cbi.handler_var }});
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
