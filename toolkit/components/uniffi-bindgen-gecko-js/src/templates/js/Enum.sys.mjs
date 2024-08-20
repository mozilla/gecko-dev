{%- if enum_.is_flat() -%}

export const {{ enum_.js_name() }} = {
    {%- for variant in enum_.variants() %}
    {{ variant.name().to_shouty_snake_case() }}: {{loop.index}},
    {%- endfor %}
};

Object.freeze({{ enum_.js_name() }});
// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        switch (dataStream.readInt32()) {
            {%- for variant in enum_.variants() %}
            case {{ loop.index }}:
                return {{ enum_.js_name() }}.{{ variant.name().to_shouty_snake_case() }}
            {%- endfor %}
            default:
                throw new UniFFITypeError("Unknown {{ enum_.js_name() }} variant");
        }
    }

    static write(dataStream, value) {
        {%- for variant in enum_.variants() %}
        if (value === {{ enum_.js_name() }}.{{ variant.name().to_shouty_snake_case() }}) {
            dataStream.writeInt32({{ loop.index }});
            return;
        }
        {%- endfor %}
        throw new UniFFITypeError("Unknown {{ enum_.js_name() }} variant");
    }

    static computeSize(value) {
        return 4;
    }

    static checkType(value) {
      if (!Number.isInteger(value) || value < 1 || value > {{ enum_.variants().len() }}) {
          throw new UniFFITypeError(`${value} is not a valid value for {{ enum_.js_name() }}`);
      }
    }
}

{%- else -%}

export class {{ enum_.js_name() }} {}
{%- for variant in enum_.variants() %}
{{enum_.js_name()}}.{{variant.name().to_upper_camel_case() }} = class extends {{ enum_.js_name() }}{
    constructor(
        {% for field in variant.fields() -%}
        {{ field.js_name() }}{%- if loop.last %}{%- else %}, {%- endif %}
        {% endfor -%}
        ) {
            super();
            {%- for field in variant.fields() %}
            this.{{field.js_name()}} = {{ field.js_name() }};
            {%- endfor %}
        }
}
{%- endfor %}

// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        switch (dataStream.readInt32()) {
            {%- for variant in enum_.variants() %}
            case {{ loop.index }}:
                return new {{ enum_.js_name() }}.{{ variant.name().to_upper_camel_case()  }}(
                    {%- for field in variant.fields() %}
                    {{ field.ffi_converter() }}.read(dataStream){%- if loop.last %}{% else %}, {%- endif %}
                    {%- endfor %}
                    );
            {%- endfor %}
            default:
                throw new UniFFITypeError("Unknown {{ enum_.js_name() }} variant");
        }
    }

    static write(dataStream, value) {
        {%- for variant in enum_.variants() %}
        if (value instanceof {{enum_.js_name()}}.{{ variant.name().to_upper_camel_case() }}) {
            dataStream.writeInt32({{ loop.index }});
            {%- for field in variant.fields() %}
            {{ field.ffi_converter() }}.write(dataStream, value.{{ field.js_name() }});
            {%- endfor %}
            return;
        }
        {%- endfor %}
        throw new UniFFITypeError("Unknown {{ enum_.js_name() }} variant");
    }

    static computeSize(value) {
        // Size of the Int indicating the variant
        let totalSize = 4;
        {%- for variant in enum_.variants() %}
        if (value instanceof {{enum_.js_name()}}.{{ variant.name().to_upper_camel_case() }}) {
            {%- for field in variant.fields() %}
            totalSize += {{ field.ffi_converter() }}.computeSize(value.{{ field.js_name() }});
            {%- endfor %}
            return totalSize;
        }
        {%- endfor %}
        throw new UniFFITypeError("Unknown {{ enum_.js_name() }} variant");
    }

    static checkType(value) {
      if (!(value instanceof {{ enum_.js_name() }})) {
        throw new UniFFITypeError(`${value} is not a subclass instance of {{ enum_.js_name() }}`);
      }
    }
}

{%- endif %}
