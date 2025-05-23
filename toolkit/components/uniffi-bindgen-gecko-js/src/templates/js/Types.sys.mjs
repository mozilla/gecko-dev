

{%- for type_def in type_definitions %}
{% match type_def %}

{%- when TypeDefinition::Simple(type_node) %}
{%- let ffi_converter = type_node.ffi_converter %}
{%- match type_node.ty %}
{%- when Type::Boolean %}
{%- include "Boolean.sys.mjs" %}

{%- when Type::UInt8 %}
{%- include "UInt8.sys.mjs" %}

{%- when Type::UInt16 %}
{%- include "UInt16.sys.mjs" %}

{%- when Type::UInt32 %}
{%- include "UInt32.sys.mjs" %}

{%- when Type::UInt64 %}
{%- include "UInt64.sys.mjs" %}

{%- when Type::Int8 %}
{%- include "Int8.sys.mjs" %}

{%- when Type::Int16 %}
{%- include "Int16.sys.mjs" %}

{%- when Type::Int32 %}
{%- include "Int32.sys.mjs" %}

{%- when Type::Int64 %}
{%- include "Int64.sys.mjs" %}

{%- when Type::Float32 %}
{%- include "Float32.sys.mjs" %}

{%- when Type::Float64 %}
{%- include "Float64.sys.mjs" %}

{%- when Type::String %}
{%- include "String.sys.mjs" %}

{%- when Type::Bytes %}
{%- include "Bytes.sys.mjs" %}

{%- else %}

throw("Unexpected type in `TypeDefinition::Simple` {{ "{type_node:?}"|format }}")

{%- endmatch %}

{%- when TypeDefinition::Optional(optional) %}
{%- include "Optional.sys.mjs" %}


{%- when TypeDefinition::Sequence(sequence) %}
{%- include "Sequence.sys.mjs" %}

{%- when TypeDefinition::Map(map) %}
{%- include "Map.sys.mjs" %}


{%- when TypeDefinition::Record(record) %}
{%- include "Record.sys.mjs" %}

{%- when TypeDefinition::Enum(e) %}
{# For enums, there are either an error *or* an enum, they can't be both. #}
{%- if e.self_type.is_used_as_error %}
{%- let error = e %}
{%- include "Error.sys.mjs" %}
{%- else %}
{%- let enum_ = e %}
{%- include "Enum.sys.mjs" %}
{%- endif %}


{%- when TypeDefinition::Interface(int) %}
{%- include "Interface.sys.mjs" %}

{%- when TypeDefinition::Custom(custom) %}
{%- include "CustomType.sys.mjs" %}

{%- when TypeDefinition::CallbackInterface(cbi) %}
{%- include "CallbackInterface.sys.mjs" %}

{%- when TypeDefinition::External(external) %}
{%- include "ExternalType.sys.mjs" %}

{%- else %}
{#- TODO implement the other types #}

{%- endmatch %}

{%- endfor %}
