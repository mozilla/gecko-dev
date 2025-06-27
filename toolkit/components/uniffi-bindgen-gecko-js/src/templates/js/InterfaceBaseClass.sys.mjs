{{ interface_base_class.js_docstring }}
export class {{ interface_base_class.name }} {
    {%- for meth in interface_base_class.methods %}
    {%- let callable = meth.callable %}
    {{ meth.js_docstring|indent(4) }}
    {% if callable.is_js_async %}async {% endif %}{{ meth.name }}({% filter indent(8) %}{% include "js/CallableArgs.sys.mjs" %}{% endfilter %}) {
      throw Error("{{ meth.name }} not implemented");
    }
    {%- endfor %}

}
