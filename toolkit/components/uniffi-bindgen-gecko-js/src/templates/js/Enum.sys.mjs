{%- if enum_.is_flat -%}

{{ enum_.js_docstring }}
export const {{ enum_.name }} = {
    {%- for variant in enum_.variants %}
    {{ variant.js_docstring|indent(4) }}
    {{ variant.name }}: {{loop.index}},
    {%- endfor %}
};
Object.freeze({{ enum_.name }});

// Export the FFIConverter object to make external types work.
export class {{ enum_|ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        switch (dataStream.readInt32()) {
            {%- for variant in enum_.variants %}
            case {{ loop.index }}:
                return {{ enum_.name }}.{{ variant.name }}
            {%- endfor %}
            default:
                throw new UniFFITypeError("Unknown {{ enum_.name }} variant");
        }
    }

    static write(dataStream, value) {
        {%- for variant in enum_.variants %}
        if (value === {{ enum_.name }}.{{ variant.name }}) {
            dataStream.writeInt32({{ loop.index }});
            return;
        }
        {%- endfor %}
        throw new UniFFITypeError("Unknown {{ enum_.name }} variant");
    }

    static computeSize(value) {
        return 4;
    }

    static checkType(value) {
      if (!Number.isInteger(value) || value < 1 || value > {{ enum_.variants.len() }}) {
          throw new UniFFITypeError(`${value} is not a valid value for {{ enum_.name }}`);
      }
    }
}

{%- else -%}

{{ enum_.js_docstring }}
export class {{ enum_.name }} {}

{%- for variant in enum_.variants %}
{{ variant.js_docstring }}
{{enum_.name }}.{{ variant.name }} = class extends {{ enum_.name }}{
    constructor(
        {% for field in variant.fields -%}
        {{ field.name }}{%- if loop.last %}{%- else %}, {%- endif %}
        {% endfor -%}
        ) {
            super();
            {%- for field in variant.fields %}
            this.{{ field.name }} = {{ field.name }};
            {%- endfor %}
        }
}
{%- endfor %}

// Export the FFIConverter object to make external types work.
export class {{ enum_|ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        switch (dataStream.readInt32()) {
            {%- for variant in enum_.variants %}
            case {{ loop.index }}:
                return new {{ enum_.name }}.{{ variant.name  }}(
                    {%- for field in variant.fields %}
                    {{ field|read_fn }}(dataStream){%- if loop.last %}{% else %}, {%- endif %}
                    {%- endfor %}
                    );
            {%- endfor %}
            default:
                throw new UniFFITypeError("Unknown {{ enum_.name }} variant");
        }
    }

    static write(dataStream, value) {
        {%- for variant in enum_.variants %}
        if (value instanceof {{ enum_.name }}.{{ variant.name }}) {
            dataStream.writeInt32({{ loop.index }});
            {%- for field in variant.fields %}
            {{ field|write_fn }}(dataStream, value.{{ field.name }});
            {%- endfor %}
            return;
        }
        {%- endfor %}
        throw new UniFFITypeError("Unknown {{ enum_.name }} variant");
    }

    static computeSize(value) {
        // Size of the Int indicating the variant
        let totalSize = 4;
        {%- for variant in enum_.variants %}
        if (value instanceof {{ enum_.name }}.{{ variant.name }}) {
            {%- for field in variant.fields %}
            totalSize += {{ field|compute_size_fn }}(value.{{ field.name }});
            {%- endfor %}
            return totalSize;
        }
        {%- endfor %}
        throw new UniFFITypeError("Unknown {{ enum_.name }} variant");
    }

    static checkType(value) {
      if (!(value instanceof {{ enum_.name }})) {
        throw new UniFFITypeError(`${value} is not a subclass instance of {{ enum_.name }}`);
      }
    }
}

{%- endif %}
