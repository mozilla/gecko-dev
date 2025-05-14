{%- for func in ci.function_definitions() %}

{{ func.js_docstring(0) -}}
export function {{ func.js_name() }}({{ func.js_arg_names() }}) {
{% call js::call_scaffolding_function(func) %}
}
{%- endfor %}
