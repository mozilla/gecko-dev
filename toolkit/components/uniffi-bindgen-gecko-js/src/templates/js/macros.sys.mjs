{%- macro call_scaffolding_function(func) %}
{%- call _call_scaffolding_function(func, func.return_type(), "", func.call_style(config)) -%}
{%- endmacro %}

{%- macro call_constructor(cons, object_type, call_style) %}
{%- call _call_scaffolding_function(cons, Some(object_type), "", call_style) -%}
{%- endmacro %}

{%- macro call_method(method, object_type, call_style) %}
{%- call _call_scaffolding_function(method, method.return_type(), object_type.ffi_converter(), call_style) -%}
{%- endmacro %}

{%- macro _call_scaffolding_function(func, return_type, receiver_ffi_converter, call_style) %}
        {%- match return_type %}
        {%- when Some with (return_type) %}
        const liftResult = (result) => {{ return_type.ffi_converter() }}.lift(result);
        {%- else %}
        const liftResult = (result) => undefined;
        {%- endmatch %}
        {%- match func.throws_type() %}
        {%- when Some with (err_type) %}
        const liftError = (data) => {{ err_type.ffi_converter() }}.lift(data);
        {%- else %}
        const liftError = null;
        {%- endmatch %}
        const functionCall = () => {
            {%- for arg in func.arguments() %}
            try {
                {{ arg.ffi_converter() }}.checkType({{ arg.js_name() }})
            } catch (e) {
                if (e instanceof UniFFITypeError) {
                    e.addItemDescriptionPart("{{ arg.js_name() }}");
                }
                throw e;
            }
            {%- endfor %}

            {%- match call_style %}
            {%- when CallStyle::AsyncWrapper %}
            return UniFFIScaffolding.callAsyncWrapper(
            {%- when CallStyle::Sync %}
            return UniFFIScaffolding.callSync(
            {%- when CallStyle::Async %}
            return UniFFIScaffolding.callAsync(
            {%- endmatch %}
                {{ function_ids.get(ci, func.ffi_func()) }}, // {{ function_ids.name(ci, func.ffi_func()) }}
                {%- if receiver_ffi_converter != "" %}
                {{ receiver_ffi_converter }}.lower(this),
                {%- endif %}
                {%- for arg in func.arguments() %}
                {{ arg.lower_fn() }}({{ arg.js_name() }}),
                {%- endfor %}
            )
        }

        {%- if call_style.is_js_async() %}
        try {
            return functionCall().then((result) => handleRustResult(result, liftResult, liftError));
        }  catch (error) {
            return Promise.reject(error)
        }
        {%- else %}
        return handleRustResult(functionCall(), liftResult, liftError);
        {%- endif %}
{%- endmacro %}
