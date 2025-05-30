/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

extern "C" {
  {%- for (preprocessor_condition, ffi_definitions, preprocessor_condition_end) in ffi_definitions.iter() %}
{{ preprocessor_condition }}
  {%- for def in ffi_definitions %}
  {%- match def %}
  {%- when FfiDefinition::RustFunction(func) %}
  {{ func.return_type.type_name }} {{ func.name.0 }}({{ func.arg_types()|join(", ") }});
  {%- when FfiDefinition::FunctionType(func) %}
  typedef {{ func.return_type.type_name }} (*{{ func.name.0 }})({{ func.arg_types()|join(", ") }});
  {%- when FfiDefinition::Struct(ffi_struct) %}
  struct {{ ffi_struct.name.0 }} {
    {%- for field in ffi_struct.fields %}
    {{ field.ty.type_name }} {{ field.name }};
    {%- endfor %}
  };
  {%- endmatch %}
  {%- endfor %}
{{ preprocessor_condition_end }}
  {%- endfor %}
}
