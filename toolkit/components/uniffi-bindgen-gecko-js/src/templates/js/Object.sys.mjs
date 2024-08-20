{%- let object = ci.get_object_definition(name).unwrap() -%}
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
    {%- if object.is_constructor_async(config) %}
    /**
     * An async constructor for {{ object.js_name() }}.
     * 
     * @returns {Promise<{{ object.js_name() }}>}: A promise that resolves
     *      to a newly constructed {{ object.js_name() }}
     */
    {%- else %}
    /**
     * A constructor for {{ object.js_name() }}.
     * 
     * @returns { {{ object.js_name() }} }
     */
    {%- endif %}
    static {{ cons.js_name() }}({{cons.js_arg_names()}}) {
        {%- call js::call_constructor(cons, type_, object.is_constructor_async(config)) -%}
    }
    {%- endfor %}

    {%- for meth in object.methods() %}

    {{ meth.js_name() }}({{ meth.js_arg_names() }}) {
        {%- call js::call_method(meth, type_, object.is_method_async(meth, config)) %}
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
