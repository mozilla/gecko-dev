{%- if enum_.is_flat() -%}

{{ enum_.js_docstring(0) -}}
export const {{ enum_.js_name() }} = {
    {%- for variant in enum_.variants() %}
    {{ variant.js_docstring(true, 4) -}}
    {{ variant.js_name(true) }}: {{loop.index}},
    {%- endfor %}
};

Object.freeze({{ enum_.js_name() }});
// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        switch (dataStream.readInt32()) {
            {%- for variant in enum_.variants() %}
            case {{ loop.index }}:
                return {{ enum_.js_name() }}.{{ variant.js_name(true) }}
            {%- endfor %}
            default:
                throw new UniFFITypeError("Unknown {{ enum_.js_name() }} variant");
        }
    }

    static write(dataStream, value) {
        {%- for variant in enum_.variants() %}
        if (value === {{ enum_.js_name() }}.{{ variant.js_name(true) }}) {
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

{{ enum_.js_docstring(0) -}}
export class {{ enum_.js_name() }} {}

{%- for variant in enum_.variants() %}
{{ variant.js_docstring(false, 0) -}}
{{enum_.js_name()}}.{{ variant.js_name(false) }} = class extends {{ enum_.js_name() }}{
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
                return new {{ enum_.js_name() }}.{{ variant.js_name(false)  }}(
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
        if (value instanceof {{enum_.js_name()}}.{{ variant.js_name(false) }}) {
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
        if (value instanceof {{enum_.js_name()}}.{{ variant.js_name(false) }}) {
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
