{%- let object = ci.get_object_definition(name).unwrap() -%}
{{ object.js_docstring(0) -}}
export class {{ object.js_name() }} {
    // Use `init` to instantiate this class.
    // DO NOT USE THIS CONSTRUCTOR DIRECTLY
    constructor(opts) {
        if (!Object.prototype.hasOwnProperty.call(opts, constructUniffiObject)) {
            throw new UniFFIError("Attempting to construct an object using the JavaScript constructor directly" +
            "Please use a UDL defined constructor, or the init function for the primary constructor")
        }
        if (!opts[constructUniffiObject] instanceof UniFFIPointer) {
            throw new UniFFIError("Attempting to create a UniFFI object with a pointer that is not an instance of UniFFIPointer")
        }
        this[uniffiObjectPtr] = opts[constructUniffiObject];
    }

    {%- for cons in object.constructors() %}
    {{ cons.js_docstring(4) -}}
    static {{ cons.js_name() }}({{cons.js_arg_names()}}) {
        {%- call js::call_constructor(cons, type_, object.use_async_wrapper_for_constructor(config)) -%}
    }
    {%- endfor %}

    {%- for meth in object.methods() %}

    {{ meth.js_docstring(4) -}}
    {{ meth.js_name() }}({{ meth.js_arg_names() }}) {
        {%- call js::call_method(meth, type_, object.use_async_wrapper_for_method(meth, config)) %}
    }
    {%- endfor %}

}

// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverter {
    static lift(value) {
        const opts = {};
        opts[constructUniffiObject] = value;
        return new {{ object.js_name() }}(opts);
    }

    static lower(value) {
        const ptr = value[uniffiObjectPtr];
        if (!(ptr instanceof UniFFIPointer)) {
            throw new UniFFITypeError("Object is not a '{{ object.js_name() }}' instance");
        }
        return ptr;
    }

    static read(dataStream) {
        return this.lift(dataStream.readPointer{{ object.js_name() }}());
    }

    static write(dataStream, value) {
        dataStream.writePointer{{ object.js_name() }}(value[uniffiObjectPtr]);
    }

    static computeSize(value) {
        return 8;
    }
}
