{{ int.js_docstring }}
export class {{ int.name }} {
    // Use `init` to instantiate this class.
    // DO NOT USE THIS CONSTRUCTOR DIRECTLY
    constructor(opts) {
        if (!Object.prototype.hasOwnProperty.call(opts, constructUniffiObject)) {
            throw new UniFFIError("Attempting to construct an int using the JavaScript constructor directly" +
            "Please use a UDL defined constructor, or the init function for the primary constructor")
        }
        if (!(opts[constructUniffiObject] instanceof UniFFIPointer)) {
            throw new UniFFIError("Attempting to create a UniFFI object with a pointer that is not an instance of UniFFIPointer")
        }
        this[uniffiObjectPtr] = opts[constructUniffiObject];
    }

    {%- for cons in int.constructors %}
    {%- let callable = cons.callable %}
    {{ cons.js_docstring|indent(4) }}
    static {% if cons.callable.is_js_async %}async {% endif %}{{ cons.name }}({% filter indent(8) %}{% include "js/CallableArgs.sys.mjs" %}{% endfilter %}) {
       {% filter indent(8) %}{% include "js/CallableBody.sys.mjs" %}{% endfilter %}
    }
    {%- endfor %}

    {%- for meth in int.methods %}
    {%- let callable = meth.callable %}

    {{ meth.js_docstring|indent(4) }}
    {% if meth.callable.is_js_async %}async {% endif %}{{ meth.name }}({% filter indent(8) %}{% include "js/CallableArgs.sys.mjs" %}{% endfilter %}) {
       {% filter indent(8) %}{% include "js/CallableBody.sys.mjs" %}{% endfilter %}
    }
    {%- endfor %}

}

{% match int.vtable -%}
{% when None -%}
// Export the FFIConverter object to make external types work.
export class {{ int|ffi_converter }} extends FfiConverter {
    static lift(value) {
        const opts = {};
        opts[constructUniffiObject] = value;
        return new {{ int.name }}(opts);
    }

    static lower(value) {
        const ptr = value[uniffiObjectPtr];
        if (!(ptr instanceof UniFFIPointer)) {
            throw new UniFFITypeError("Object is not a '{{ int.name }}' instance");
        }
        return ptr;
    }

    static lowerReceiver(value) {
        // This works exactly the same as lower for non-trait interfaces
        return this.lower(value);
    }

    static read(dataStream) {
        return this.lift(dataStream.readPointer({{ int.object_id }}));
    }

    static write(dataStream, value) {
        dataStream.writePointer({{ int.object_id }}, this.lower(value));
    }

    static computeSize(value) {
        return 8;
    }
}
{% when Some(vtable) -%}
// FfiConverter for a trait interface.  This is a hybrid of the FFIConverter regular interfaces and
// for callback interfaces.
//
// Export the FFIConverter object to make external types work.
export class {{ int|ffi_converter }} extends FfiConverter {
    // lift works like a regular interface
    static lift(value) {
        const opts = {};
        opts[constructUniffiObject] = value;
        return new {{ int.name }}(opts);
    }

    // lower treats value like a callback interface
    static lower(value) {
        return {{ vtable.js_handler_var }}.storeCallbackObj(value)
    }

    // lowerReceiver is used when calling methods on an interface we got from Rust, 
    // it treats value like a regular interface.
    static lowerReceiver(value) {
        const ptr = value[uniffiObjectPtr];
        if (!(ptr instanceof UniFFIPointer)) {
            throw new UniFFITypeError("Object is not a '{{ int.name }}' instance");
        }
        return ptr;
    }

    static read(dataStream) {
        return this.lift(dataStream.readPointer({{ int.object_id }}));
    }

    static write(dataStream, value) {
        dataStream.writePointer({{ int.object_id }}, this.lower(value));
    }

    static computeSize(value) {
        return 8;
    }
}

{% include "CallbackInterfaceHandler.sys.mjs" -%}
{% endmatch %}
