/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module generates the UniFFI C++ code that sits between the Rust scaffolding code and the
//! generated JS code.  The main responsibility of the generated C++ code is to implement the
//! UniFFI WebIDL interface.
//!
//! The general strategy is to take the generalized component interface and convert it into types
//! that can be easily rendered by the UniFFIScaffolding.cpp template -- an intermediate
//! representation of sorts.
//!
//! In many cases this means converting a type from `uniffi_bindgen::interface` into another type
//! that represents the same concept, but is easier for the templates to render.  In this case, the
//! new type name has `Cpp` appended to it ([FfiFunction] is converted to [FfiFunctionCpp]).

use crate::{CallbackIds, Component, FunctionIds, ObjectIds};
use askama::Template;
use heck::{ToShoutySnakeCase, ToUpperCamelCase};
use uniffi_bindgen::interface::{ComponentInterface, FfiFunction, FfiType};

#[derive(Template)]
#[template(path = "UniFFIScaffolding.cpp", escape = "none")]
pub struct CPPScaffoldingTemplate<'a> {
    // Prefix for each function name in.  This is related to how we handle the test fixtures.  For
    // each function defined in the UniFFI namespace in UniFFI.webidl we:
    //   - Generate a function in to handle it using the real UDL files
    //   - Generate a different function in for handle it using the fixture UDL files
    //   - Have a hand-written stub function that always calls the first function and only calls
    //     the second function in if MOZ_UNIFFI_FIXTURES is defined.
    prefix: &'a str,
    ffi_functions: Vec<FfiFunctionCpp>,
    pointer_types: Vec<PointerType>,
    callback_interfaces: Vec<CallbackInterfaceCpp>,
    scaffolding_calls: Vec<ScaffoldingCall>,
}

impl<'a> CPPScaffoldingTemplate<'a> {
    pub fn new(
        prefix: &'a str,
        components: &'a Vec<Component>,
        function_ids: &'a FunctionIds<'a>,
        object_ids: &'a ObjectIds<'a>,
        callback_ids: &'a CallbackIds<'a>,
    ) -> Self {
        Self {
            prefix,
            ffi_functions: Self::ffi_functions(components),
            pointer_types: Self::pointer_types(object_ids, components),
            callback_interfaces: Self::callback_interfaces(prefix, callback_ids, components),
            scaffolding_calls: Self::scaffolding_calls(prefix, function_ids, components),
        }
    }

    fn ffi_functions(components: &[Component]) -> Vec<FfiFunctionCpp> {
        components
            .iter()
            .flat_map(|c| c.ci.iter_user_ffi_function_definitions())
            .map(|ffi_func| FfiFunctionCpp {
                name: ffi_func.name().to_string(),
                arg_types: ffi_func
                    .arguments()
                    .iter()
                    .map(|a| cpp_type(&a.type_()))
                    .chain(["RustCallStatus*".to_owned()])
                    .collect(),
                return_type: match ffi_func.return_type() {
                    Some(t) => cpp_type(t),
                    None => "void".to_owned(),
                },
            })
            .collect()
    }

    fn pointer_types(object_ids: &ObjectIds<'_>, components: &[Component]) -> Vec<PointerType> {
        components
            .iter()
            .flat_map(|c| {
                c.ci.object_definitions()
                    .iter()
                    .map(move |obj| PointerType {
                        object_id: object_ids.get(&c.ci, obj),
                        name: pointer_type(&c.ci, obj.name()),
                        label: format!("{}::{}", c.ci.namespace(), obj.name()),
                        clone_fn: obj.ffi_object_clone().name().to_string(),
                        free_fn: obj.ffi_object_free().name().to_string(),
                    })
            })
            .collect()
    }

    fn callback_interfaces(
        prefix: &str,
        callback_ids: &CallbackIds<'_>,
        components: &[Component],
    ) -> Vec<CallbackInterfaceCpp> {
        components
            .iter()
            .flat_map(|c| {
                c.ci.callback_interface_definitions()
                    .iter()
                    .map(move |cbi| CallbackInterfaceCpp {
                        id: callback_ids.get(&c.ci, cbi),
                        name: format!("{}:{}", c.ci.namespace(), cbi.name()),
                        handler_fn: format!(
                            "{prefix}CallbackHandler{}",
                            cbi.name().to_upper_camel_case()
                        ),
                        static_var: format!(
                            "JS_CALLBACK_HANDLER_{}",
                            cbi.name().to_shouty_snake_case(),
                        ),
                        init_fn: cbi.ffi_init_callback().name().to_owned(),
                    })
            })
            .collect()
    }

    fn scaffolding_calls(
        prefix: &str,
        function_ids: &'a FunctionIds<'a>,
        components: &[Component],
    ) -> Vec<ScaffoldingCall> {
        let mut calls: Vec<ScaffoldingCall> = components
            .iter()
            .flat_map(|c| {
                exposed_functions(&c.ci).map(move |ffi_func| {
                    ScaffoldingCall::new(prefix, &c.ci, ffi_func, function_ids)
                })
            })
            .collect();
        calls.sort_by_key(|c| c.function_id);
        calls
    }
}

struct FfiFunctionCpp {
    name: String,
    arg_types: Vec<String>,
    return_type: String,
}

struct PointerType {
    object_id: usize,
    name: String,
    label: String,
    clone_fn: String,
    free_fn: String,
}

struct CallbackInterfaceCpp {
    id: usize,
    name: String,
    handler_fn: String,
    static_var: String,
    init_fn: String,
}

struct ScaffoldingCall {
    handler_class_name: String,
    function_id: usize,
    ffi_func_name: String,
    return_type: Option<ScaffoldingCallReturnType>,
    arguments: Vec<ScaffoldingCallArgument>,
}

impl ScaffoldingCall {
    fn new(
        prefix: &str,
        ci: &ComponentInterface,
        ffi_func: &FfiFunction,
        function_ids: &FunctionIds,
    ) -> Self {
        let handler_class_name = format!(
            "ScaffoldingCallHandler{prefix}{}",
            ffi_func.name().to_upper_camel_case()
        );
        let arguments = ffi_func
            .arguments()
            .into_iter()
            .map(|a| ScaffoldingCallArgument {
                var_name: format!("m{}", a.name().to_upper_camel_case()),
                scaffolding_converter: scaffolding_converter(ci, &a.type_()),
            })
            .collect::<Vec<_>>();

        Self {
            handler_class_name,
            function_id: function_ids.get(ci, ffi_func),
            ffi_func_name: ffi_func.name().to_owned(),
            return_type: ffi_func
                .return_type()
                .map(|return_type| ScaffoldingCallReturnType {
                    scaffolding_converter: scaffolding_converter(ci, &return_type),
                }),
            arguments,
        }
    }
}

struct ScaffoldingCallReturnType {
    scaffolding_converter: String,
}

struct ScaffoldingCallArgument {
    var_name: String,
    scaffolding_converter: String,
}

fn scaffolding_converter(ci: &ComponentInterface, ffi_type: &FfiType) -> String {
    match ffi_type {
        FfiType::RustArcPtr(name) => {
            format!("ScaffoldingObjectConverter<&{}>", pointer_type(ci, name),)
        }
        _ => format!("ScaffoldingConverter<{}>", cpp_type(ffi_type)),
    }
}

fn pointer_type(ci: &ComponentInterface, name: &str) -> String {
    format!(
        "k{}{}PointerType",
        ci.namespace().to_upper_camel_case(),
        name.to_upper_camel_case()
    )
}

// Type for the Rust scaffolding code
fn cpp_type(ffi_type: &FfiType) -> String {
    match ffi_type {
        FfiType::UInt8 => "uint8_t".to_owned(),
        FfiType::Int8 => "int8_t".to_owned(),
        FfiType::UInt16 => "uint16_t".to_owned(),
        FfiType::Int16 => "int16_t".to_owned(),
        FfiType::UInt32 => "uint32_t".to_owned(),
        FfiType::Int32 => "int32_t".to_owned(),
        FfiType::UInt64 => "uint64_t".to_owned(),
        FfiType::Int64 => "int64_t".to_owned(),
        FfiType::Float32 => "float".to_owned(),
        FfiType::Float64 => "double".to_owned(),
        FfiType::RustBuffer(_) => "RustBuffer".to_owned(),
        FfiType::RustArcPtr(_) => "void*".to_owned(),
        FfiType::ForeignBytes => unimplemented!("ForeignBytes not supported"),
        FfiType::Handle => "uint64_t".to_owned(),
        FfiType::RustCallStatus => "RustCallStatus".to_owned(),
        FfiType::Callback(name) | FfiType::Struct(name) => name.to_owned(),
        FfiType::VoidPointer => "void*".to_owned(),
        FfiType::Reference(_) => unimplemented!("References not supported"),
    }
}

/// Get scaffolding call functions exposed to JS through the `UniFFIScaffolding` WebIDL interface
pub fn exposed_functions(ci: &ComponentInterface) -> impl Iterator<Item = &FfiFunction> {
    ci.function_definitions()
        .into_iter()
        .map(|f| f.ffi_func())
        .chain(ci.object_definitions().into_iter().flat_map(|o| {
            o.methods()
                .into_iter()
                .map(|m| m.ffi_func())
                .chain(o.constructors().into_iter().map(|c| c.ffi_func()))
        }))
}
