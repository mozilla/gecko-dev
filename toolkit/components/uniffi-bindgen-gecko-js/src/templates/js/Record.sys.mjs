{{ record.js_docstring }}
export class {{ record.name }} {
    constructor(
        {
            {%- for field in record.fields %}
            {{ field.name }}{% if let Some(lit) = field.default %}= {{ lit.js_lit }}{% endif %}
            {%- if !loop.last %}, {% endif %}
            {%- endfor %}
        } = {
            {%- for field in record.fields %}
            {{ field.name }}: undefined
            {%- if !loop.last %}, {% endif %}
            {%- endfor %}
        }
    ) {
        {%- for field in record.fields %}
        try {
            {{ field|check_type_fn }}({{ field.name }})
        } catch (e) {
            if (e instanceof UniFFITypeError) {
                e.addItemDescriptionPart("{{ field.name }}");
            }
            throw e;
        }
        {%- endfor %}

        {%- for field in record.fields %}
        {{ field.js_docstring|indent(8) }}
        this.{{field.name}} = {{ field.name }};
        {%- endfor %}
    }

    equals(other) {
        return (
            {%- for field in record.fields %}
            {% if !loop.first %}&& {% endif %}{{ field|field_equals("this", "other") }}
            {%- endfor %}
        )
    }
}

// Export the FFIConverter object to make external types work.
export class {{ record|ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        return new {{record.name}}({
            {%- for field in record.fields %}
            {{ field.name }}: {{ field|read_fn }}(dataStream),
            {%- endfor %}
        });
    }
    static write(dataStream, value) {
        {%- for field in record.fields %}
        {{ field|write_fn }}(dataStream, value.{{field.name}});
        {%- endfor %}
    }

    static computeSize(value) {
        let totalSize = 0;
        {%- for field in record.fields %}
        totalSize += {{ field|compute_size_fn }}(value.{{ field.name }});
        {%- endfor %}
        return totalSize
    }

    static checkType(value) {
        super.checkType(value);
        if (!(value instanceof {{ record.name }})) {
            throw new UniFFITypeError(`Expected '{{ record.name }}', found '${typeof value}'`);
        }
        {%- for field in record.fields %}
        try {
            {{ field|check_type_fn }}(value.{{ field.name }});
        } catch (e) {
            if (e instanceof UniFFITypeError) {
                e.addItemDescriptionPart(".{{ field.name }}");
            }
            throw e;
        }
        {%- endfor %}
    }
}
