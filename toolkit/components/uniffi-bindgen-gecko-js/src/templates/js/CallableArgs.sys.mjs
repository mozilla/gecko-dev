{%- for arg in callable.arguments %}
{{ arg.name }}{% if let Some(lit) = arg.default %} = {{ lit.js_lit }}{% endif %}
{%- if !loop.last %}, {% endif %}
{%- endfor -%}
