

{%- for type_def in type_definitions %}
{% match type_def %}

{%- when TypeDefinition::Simple(type_node) %}
{# No code needed, the ffi converter class is defined in the shared UniFFI.sys.mjs module #}

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
