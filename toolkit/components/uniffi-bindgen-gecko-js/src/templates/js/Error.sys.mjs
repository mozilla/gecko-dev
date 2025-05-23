{%- let string_type = Type::String %}
{%- let string_ffi_converter = string_type.ffi_converter() %}

{{ error.js_docstring(0) -}}
export class {{ error.js_name() }} extends Error {}
{% for variant in error.variants() %}

{{ variant.js_docstring(error.is_flat(), 0) -}}
export class {{ variant.name().to_upper_camel_case() }} extends {{ error.js_name() }} {
{% if error.is_flat() %}
    constructor(message, ...params) {
        super(...params);
        this.message = message;
    }
{%- else %}
    constructor(
        {% for field in variant.fields() -%}
        {{field.js_name()}},
        {% endfor -%}
        ...params
    ) {
        {%- if !variant.fields().is_empty() %}
        const message = `{% for field in variant.fields() %}{{ field.js_name() }}: ${ {{ field.js_name() }} }{% if !loop.last %}, {% endif %}{% endfor %}`;
        super(message, ...params);
        {%- else %}
        super(...params);
        {%- endif %}
        {%- for field in variant.fields() %}
        this.{{field.js_name()}} = {{ field.js_name() }};
        {%- endfor %}
    }
{%- endif %}
    toString() {
        return `{{ variant.name().to_upper_camel_case() }}: ${super.toString()}`
    }
}
{%- endfor %}

// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        switch (dataStream.readInt32()) {
            {%- for variant in error.variants() %}
            case {{ loop.index }}:
                {%- if error.is_flat() %}
                return new {{ variant.name().to_upper_camel_case()  }}({{ string_ffi_converter }}.read(dataStream));
                {%- else %}
                return new {{ variant.name().to_upper_camel_case()  }}(
                    {%- for field in variant.fields() %}
                    {{ field.ffi_converter() }}.read(dataStream){%- if loop.last %}{% else %}, {%- endif %}
                    {%- endfor %}
                    );
                {%- endif %}
            {%- endfor %}
            default:
                throw new UniFFITypeError("Unknown {{ error.js_name() }} variant");
        }
    }
    static computeSize(value) {
        // Size of the Int indicating the variant
        let totalSize = 4;
        {%- for variant in error.variants() %}
        if (value instanceof {{ variant.name().to_upper_camel_case() }}) {
            {%- for field in variant.fields() %}
            totalSize += {{ field.ffi_converter() }}.computeSize(value.{{ field.js_name() }});
            {%- endfor %}
            return totalSize;
        }
        {%- endfor %}
        throw new UniFFITypeError("Unknown {{ error.js_name() }} variant");
    }
    static write(dataStream, value) {
        {%- for variant in error.variants() %}
        if (value instanceof {{ variant.name().to_upper_camel_case() }}) {
            dataStream.writeInt32({{ loop.index }});
            {%- for field in variant.fields() %}
            {{ field.ffi_converter() }}.write(dataStream, value.{{ field.js_name() }});
            {%- endfor %}
            return;
        }
        {%- endfor %}
        throw new UniFFITypeError("Unknown {{ error.js_name() }} variant");
    }

    static errorClass = {{ error.js_name() }};
}
