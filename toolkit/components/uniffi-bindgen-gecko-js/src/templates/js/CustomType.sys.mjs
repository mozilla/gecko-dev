export class {{ custom.self_type.ffi_converter }} extends FfiConverter {
    static lift(value) {
        {%- if let Some(lift_expr) = custom.lift_expr %}
        const builtinVal = {{ custom.builtin.ffi_converter }}.lift(value);
        return {{ lift_expr }};
        {%- else %}
        return {{ custom.builtin.ffi_converter }}.lift(value);
        {%- endif %}
    }

    static lower(value) {
        {%- if let Some(lower_expr) = custom.lower_expr %}
        const builtinVal = {{ lower_expr }};
        return {{ custom.builtin.ffi_converter }}.lower(builtinVal);
        {%- else %}
        return {{ custom.builtin.ffi_converter }}.lower(value);
        {%- endif %}
    }

    static write(dataStream, value) {
        {%- if let Some(lower_expr) = custom.lower_expr %}
        const builtinVal = {{ lower_expr }};
        {{ custom.builtin.ffi_converter }}.write(dataStream, builtinVal);
        {%- else %}
        {{ custom.builtin.ffi_converter }}.write(dataStream, value);
        {%- endif %}
    }

    static read(dataStream) {
        const builtinVal = {{ custom.builtin.ffi_converter }}.read(dataStream);
        {%- if let Some(lift_expr) = custom.lift_expr %}
        return {{ lift_expr }};
        {%- else %}
        return builtinVal;
        {%- endif %}
    }

    static computeSize(value) {
        {%- if let Some(lower_expr) = custom.lower_expr %}
        const builtinVal = {{ lower_expr }};
        return {{ custom.builtin.ffi_converter }}.computeSize(builtinVal);
        {%- else %}
        return {{ custom.builtin.ffi_converter }}.computeSize(value);
        {%- endif %}
    }

    static checkType(value) {
        if (value === null || value === undefined) {
            throw new TypeError("value is null or undefined");
        }
        {%- if let Some(type_name) = &custom.type_name %}
        if (value?.constructor?.name !== "{{ type_name }}") {
            throw new TypeError(`${value} is not a {{ type_name }}`);
        }
        {%- endif %}
    }
}
