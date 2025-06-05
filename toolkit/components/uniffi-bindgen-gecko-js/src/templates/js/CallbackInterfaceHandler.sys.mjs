const {{ vtable.js_handler_var }} = new UniFFICallbackHandler(
    "{{ vtable.interface_name }}",
    {{ vtable.callback_interface_id }},
    [
        {%- for vtable_method in vtable.methods %}
        new UniFFICallbackMethodHandler(
            "{{ vtable_method.callable.name }}",
            [
                {%- for arg in vtable_method.callable.arguments %}
                {{ arg|ffi_converter }},
                {%- endfor %}
            ],
            {%- match vtable_method.callable.return_type.ty %}
            {%- when Some(return_type) %}
            {{ return_type|lower_fn }}.bind({{ return_type|ffi_converter }}),
            {%- when None %}
            (result) => undefined,
            {%- endmatch %}

            {%- match vtable_method.callable.throws_type.ty %}
            {%- when Some(err_type) %}
            (e) => {
              if (e instanceof {{ err_type|class_name }}) {
                return {{ err_type|lower_fn }}(e);
              }
              throw e;
            }
            {%- when None %}
            (e) => {
              throw e;
            }
            {%- endmatch %}
        ),
        {%- endfor %}
    ]
);

// Allow the shutdown-related functionality to be tested in the unit tests
UnitTestObjs.{{ vtable.js_handler_var }} = {{ vtable.js_handler_var }};
