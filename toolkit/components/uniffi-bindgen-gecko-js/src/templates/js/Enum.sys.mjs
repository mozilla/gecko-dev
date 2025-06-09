{%- if enum_.is_flat -%}

{{ enum_.js_docstring }}
export const {{ enum_.name }} = {
    {%- for variant in enum_.variants %}
    {{ variant.js_docstring|indent(4) }}
    {{ variant.name }}: {{ variant.discr.js_lit }},
    {%- endfor %}
};
Object.freeze({{ enum_.name }});

// Export the FFIConverter object to make external types work.
export class {{ enum_.self_type.ffi_converter }} extends FfiConverterArrayBuffer {
    static #validValues = Object.values({{ enum_.name }})

    static read(dataStream) {
        // Use sequential indices (1-based) for the wire format to match the Rust scaffolding
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
        // Use sequential indices (1-based) for the wire format to match the Rust scaffolding
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
      // Check that the value is a valid enum variant
      if (!this.#validValues.includes(value)) {
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
        {%- if variant.fields.is_empty() -%}
        ) {
            super();
        {%- else -%}
            {
                {%- for field in variant.fields -%}
                {{ field.name }}{% if let Some(default_val) = field.default %} = {{ default_val.js_lit }}{% else %} = undefined{% endif %}{%- if loop.last %} {% else %}, {% endif -%}
            {%- endfor -%}
            } = {}) {
                super();
            {% for field in variant.fields -%}
            try {
                {{ field.ty.ffi_converter }}.checkType({{ field.name }});
            } catch (e) {
                if (e instanceof UniFFITypeError) {
                    e.addItemDescriptionPart("{{ field.name }}");
                }
                throw e;
            }
            this.{{ field.name }} = {{ field.name }};
            {%- endfor %}
        {%- endif %}
    }
}
{%- endfor %}

// Export the FFIConverter object to make external types work.
export class {{ enum_.self_type.ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        // Use sequential indices (1-based) for the wire format to match the Rust scaffolding
        switch (dataStream.readInt32()) {
            {%- for variant in enum_.variants %}
            case {{ loop.index }}:
                {%- if variant.fields.is_empty() %}
                return new {{ enum_.name }}.{{ variant.name }}();
                {%- else %}
                return new {{ enum_.name }}.{{ variant.name }}({
                    {%- for field in variant.fields %}
                    {{ field.name }}: {{ field.ty.ffi_converter }}.read(dataStream){%- if loop.last %}{% else %}, {%- endif %}
                    {%- endfor %}
                });
                {%- endif %}
            {%- endfor %}
            default:
                throw new UniFFITypeError("Unknown {{ enum_.name }} variant");
        }
    }

    static write(dataStream, value) {
        // Use sequential indices (1-based) for the wire format to match the Rust scaffolding
        {%- for variant in enum_.variants %}
        if (value instanceof {{ enum_.name }}.{{ variant.name }}) {
            dataStream.writeInt32({{ loop.index }});
            {%- for field in variant.fields %}
            {{ field.ty.ffi_converter }}.write(dataStream, value.{{ field.name }});
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
            totalSize += {{ field.ty.ffi_converter }}.computeSize(value.{{ field.name }});
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
