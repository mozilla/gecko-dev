// Export the FFIConverter object to make external types work.
export class {{ cbi|ffi_converter }} extends FfiConverter {
    static lower(callbackObj) {
        return {{ cbi.js_handler_var }}.storeCallbackObj(callbackObj)
    }

    static lift(handleId) {
        return {{ cbi.js_handler_var }}.getCallbackObj(handleId)
    }

    static read(dataStream) {
        return this.lift(dataStream.readInt64())
    }

    static write(dataStream, callbackObj) {
        dataStream.writeInt64(this.lower(callbackObj))
    }

    static computeSize(callbackObj) {
        return 8;
    }
}

const {{ cbi.js_handler_var }} = new UniFFICallbackHandler(
    "{{ cbi.name }}",
    {{ cbi.id }},
    [
        {%- for vtable_method in cbi.vtable.methods %}
        new UniFFICallbackMethodHandler(
            "{{ vtable_method.callable.name }}",
            [
                {%- for arg in vtable_method.callable.arguments %}
                {{ arg|ffi_converter }},
                {%- endfor %}
            ],
        ),
        {%- endfor %}
    ]
);

// Allow the shutdown-related functionality to be tested in the unit tests
UnitTestObjs.{{ cbi.js_handler_var }} = {{ cbi.js_handler_var }};
