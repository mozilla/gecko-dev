{%- for arg in callable.arguments %}
{{ arg|check_type_fn }}({{ arg.name }});
{%- endfor %}
const result = {% if callable.is_js_async %}await {% endif %}{{ callable.uniffi_scaffolding_method }}(
    {{ callable.id }}, // {{ callable.ffi_func.0 }}
    {%- if let CallableKind::Method { ffi_converter, .. } = callable.kind %}
    {{ ffi_converter }}.lower(this),
    {%- endif %}
    {%- for arg in callable.arguments %}
    {{ arg|lower_fn }}({{ arg.name }}),
    {%- endfor %}
)
return handleRustResult(
    result,

    {%- match callable.return_type.ty %}
    {%- when Some(return_type) %}
    {{ return_type|lift_fn }}.bind({{ return_type|ffi_converter }}),
    {%- when None %}
    (result) => undefined,
    {%- endmatch %}

    {%- match callable.throws_type.ty %}
    {%- when Some(err_type) %}
    {{ err_type|lift_fn }}.bind({{ err_type|ffi_converter }}),
    {%- when None %}
    null,
    {%- endmatch %}
)
