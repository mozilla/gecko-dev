{{ error.js_docstring }}
export class {{ error.name }} extends Error {}
{% for variant in error.variants %}

{{ variant.js_docstring }}
export class {{ variant.name }} extends {{ error.name }} {
{% if error.is_flat %}
    constructor(message, ...params) {
        super(...params);
        this.message = message;
    }
{%- else %}
    constructor(
        {% for field in variant.fields -%}
        {{ field.name }},
        {% endfor -%}
        ...params
    ) {
        {%- if !variant.fields.is_empty() %}
        const message = `{% for field in variant.fields %}{{ field.name }}: ${ {{ field.name }} }{% if !loop.last %}, {% endif %}{% endfor %}`;
        super(message, ...params);
        {%- else %}
        super(...params);
        {%- endif %}
        {%- for field in variant.fields %}
        this.{{field.name}} = {{ field.name }};
        {%- endfor %}
    }
{%- endif %}
    toString() {
        return `{{ variant.name }}: ${super.toString()}`
    }
}
{%- endfor %}

// Export the FFIConverter object to make external types work.
export class {{ error.self_type.ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        switch (dataStream.readInt32()) {
            {%- for variant in error.variants %}
            case {{ loop.index }}:
                {%- if error.is_flat %}
                return new {{ variant.name  }}({{ string_type_node.ffi_converter }}.read(dataStream));
                {%- else %}
                return new {{ variant.name  }}(
                    {%- for field in variant.fields %}
                    {{ field.ty.ffi_converter }}.read(dataStream){%- if loop.last %}{% else %}, {%- endif %}
                    {%- endfor %}
                    );
                {%- endif %}
            {%- endfor %}
            default:
                throw new UniFFITypeError("Unknown {{ error.name }} variant");
        }
    }
    static computeSize(value) {
        // Size of the Int indicating the variant
        let totalSize = 4;
        {%- for variant in error.variants %}
        if (value instanceof {{ variant.name }}) {
            {%- for field in variant.fields %}
            totalSize += {{ field.ty.ffi_converter }}.computeSize(value.{{ field.name }});
            {%- endfor %}
            return totalSize;
        }
        {%- endfor %}
        throw new UniFFITypeError("Unknown {{ error.name }} variant");
    }
    static write(dataStream, value) {
        {%- for variant in error.variants %}
        if (value instanceof {{ variant.name }}) {
            dataStream.writeInt32({{ loop.index }});
            {%- for field in variant.fields %}
            {{ field.ty.ffi_converter }}.write(dataStream, value.{{ field.name }});
            {%- endfor %}
            return;
        }
        {%- endfor %}
        throw new UniFFITypeError("Unknown {{ error.name }} variant");
    }

    static errorClass = {{ error.name }};
}
