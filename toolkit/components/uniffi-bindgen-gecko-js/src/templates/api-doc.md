# {{ module_name }}

{% for class in classes -%}
```{js:autoclass} {{ jsdoc_module_name }}.{{ class }}
    :members:
    :exclude-members: {{ class }}
```
{% endfor -%}
{% for function in functions -%}
```{js:autofunction} {{ jsdoc_module_name }}.{{ function }}
```
{% endfor -%}
