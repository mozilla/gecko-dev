{%- let record = ci.get_record_definition(name).unwrap() -%}
{{ record.js_docstring(0) -}}
export class {{ record.js_name() }} {
    constructor({{ record.constructor_field_list() }} = {}) {
        {%- for field in record.fields() %}
        try {
            {{ field.ffi_converter() }}.checkType({{ field.js_name() }})
        } catch (e) {
            if (e instanceof UniFFITypeError) {
                e.addItemDescriptionPart("{{ field.js_name() }}");
            }
            throw e;
        }
        {%- endfor %}

        {%- for field in record.fields() %}
        {{ field.js_docstring(8) -}}
        this.{{field.js_name()}} = {{ field.js_name() }};
        {%- endfor %}
    }

    equals(other) {
        return (
            {%- for field in record.fields() %}
            {{ field.as_type().equals("this.{}"|format(field.js_name()), "other.{}"|format(field.js_name())) }}{% if !loop.last %} &&{% endif %}
            {%- endfor %}
        )
    }
}

// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        return new {{record.js_name()}}({
            {%- for field in record.fields() %}
            {{ field.js_name() }}: {{ field.read_datastream_fn() }}(dataStream),
            {%- endfor %}
        });
    }
    static write(dataStream, value) {
        {%- for field in record.fields() %}
        {{ field.write_datastream_fn() }}(dataStream, value.{{field.js_name()}});
        {%- endfor %}
    }

    static computeSize(value) {
        let totalSize = 0;
        {%- for field in record.fields() %}
        totalSize += {{ field.ffi_converter() }}.computeSize(value.{{ field.js_name() }});
        {%- endfor %}
        return totalSize
    }

    static checkType(value) {
        super.checkType(value);
        if (!(value instanceof {{ record.js_name() }})) {
            throw new UniFFITypeError(`Expected '{{ record.js_name() }}', found '${typeof value}'`);
        }
        {%- for field in record.fields() %}
        try {
            {{ field.ffi_converter() }}.checkType(value.{{ field.js_name() }});
        } catch (e) {
            if (e instanceof UniFFITypeError) {
                e.addItemDescriptionPart(".{{ field.js_name() }}");
            }
            throw e;
        }
        {%- endfor %}
    }
}
