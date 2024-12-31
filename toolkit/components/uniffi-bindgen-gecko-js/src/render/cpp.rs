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

use std::collections::HashSet;

use askama::Template;
use heck::{ToSnakeCase, ToUpperCamelCase};
use uniffi_bindgen::interface::{
    AsType, Callable, CallbackInterface, ComponentInterface, FfiDefinition, FfiFunction, FfiType,
};

use super::shared::*;
use crate::{CallbackIds, Component, FunctionIds, ObjectIds};

#[derive(Template)]
#[template(path = "UniFFIScaffolding.cpp", escape = "none")]
pub struct CPPScaffoldingTemplate {
    all_ffi_definitions: CombinedItems<Vec<FfiDefinitionCpp>>,
    all_pointer_types: CombinedItems<Vec<PointerType>>,
    all_callback_interfaces: CombinedItems<Vec<CallbackInterfaceCpp>>,
    all_scaffolding_calls: CombinedItems<Vec<ScaffoldingCall>>,
}

impl CPPScaffoldingTemplate {
    pub fn new(
        components: &[Component],
        fixture_components: &[Component],
        function_ids: &FunctionIds<'_>,
        object_ids: &ObjectIds<'_>,
        callback_ids: &CallbackIds<'_>,
    ) -> Self {
        Self {
            all_ffi_definitions: Self::all_ffi_definitions(components, fixture_components),
            all_pointer_types: CombinedItems::new(
                Self::pointer_types(object_ids, components),
                Self::pointer_types(object_ids, fixture_components),
            ),
            all_callback_interfaces: CombinedItems::new(
                Self::callback_interfaces(callback_ids, components),
                Self::callback_interfaces(callback_ids, fixture_components),
            ),
            all_scaffolding_calls: CombinedItems::new(
                Self::scaffolding_calls(function_ids, components),
                Self::scaffolding_calls(function_ids, fixture_components),
            ),
        }
    }

    fn all_ffi_definitions(
        components: &[Component],
        fixture_components: &[Component],
    ) -> CombinedItems<Vec<FfiDefinitionCpp>> {
        // Track which FFI definition's we've seen and don't add them twice.
        // This avoids duplicate definitions for shared FFI types like `CallbackInterfaceFree`.
        //
        // The code below ordered so that duplicated definitions get added to the components side
        // of `CombinedItems` rather than the fixtures side.  This way if fixtures are disabled, we
        // don't see missing definition errors.
        let mut seen_names = HashSet::new();

        CombinedItems::new(
            Self::ffi_definitions(components)
                .into_iter()
                .filter(|ffi_def| seen_names.insert(ffi_def.name().to_owned()))
                .collect(),
            Self::ffi_definitions(fixture_components)
                .into_iter()
                .filter(|ffi_def| seen_names.insert(ffi_def.name().to_owned()))
                .collect(),
        )
    }

    fn ffi_definitions(components: &[Component]) -> Vec<FfiDefinitionCpp> {
        components
            .iter()
            .flat_map(|c| c.ci.ffi_definitions())
            .map(|ffi_definition| match ffi_definition {
                FfiDefinition::Function(ffi_func) => FfiDefinitionCpp::Function(FfiFunctionCpp {
                    name: ffi_func.name().to_snake_case(),
                    arg_types: ffi_func
                        .arguments()
                        .iter()
                        .map(|a| cpp_type(&a.type_()))
                        .chain(
                            ffi_func
                                .has_rust_call_status_arg()
                                .then(|| "RustCallStatus*".to_owned()),
                        )
                        .collect(),
                    return_type: return_type(ffi_func.return_type()),
                }),
                FfiDefinition::CallbackFunction(ffi_callback) => {
                    FfiDefinitionCpp::CallbackFunction(FfiCallbackFunctionCpp {
                        name: ffi_callback.name().to_upper_camel_case(),
                        arg_types: ffi_callback
                            .arguments()
                            .into_iter()
                            .map(|a| cpp_type(&a.type_()))
                            .chain(
                                ffi_callback
                                    .has_rust_call_status_arg()
                                    .then(|| "RustCallStatus*".to_owned()),
                            )
                            .collect(),
                        return_type: return_type(ffi_callback.return_type()),
                    })
                }
                FfiDefinition::Struct(ffi_struct) => FfiDefinitionCpp::Struct(FfiStructCpp {
                    name: ffi_struct.name().to_upper_camel_case(),
                    fields: ffi_struct
                        .fields()
                        .into_iter()
                        .map(|f| FfiFieldCpp {
                            name: f.name().to_snake_case(),
                            type_: cpp_type(&f.type_()),
                        })
                        .collect(),
                }),
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
                        name: pointer_type(&c.ci.namespace(), obj.name()),
                        label: format!("{}::{}", c.ci.namespace(), obj.name()),
                        clone_fn: obj.ffi_object_clone().name().to_string(),
                        free_fn: obj.ffi_object_free().name().to_string(),
                    })
            })
            .collect()
    }

    fn callback_interfaces(
        callback_ids: &CallbackIds<'_>,
        components: &[Component],
    ) -> Vec<CallbackInterfaceCpp> {
        components
            .iter()
            .flat_map(|c| {
                c.ci.callback_interface_definitions()
                    .iter()
                    .map(move |cbi| {
                        let cbi_name = cbi.name().to_upper_camel_case();
                        CallbackInterfaceCpp {
                            id: callback_ids.get(&c.ci, cbi),
                            name: format!("{}:{}", c.ci.namespace(), cbi.name()),
                            js_handler_var: format!("gCallbackInterfaceJsHandler{cbi_name}"),
                            vtable: Self::callback_interface_vtable(&c.ci, cbi),
                            free_fn: format!("callbackInterfaceFree{cbi_name}"),
                            init_fn: cbi.ffi_init_callback().name().to_owned(),
                        }
                    })
            })
            .collect()
    }

    fn callback_interface_vtable(
        ci: &ComponentInterface,
        cbi: &CallbackInterface,
    ) -> CallbackInterfaceVTable {
        let cbi_name = cbi.name().to_upper_camel_case();
        let cbi_name_snake = cbi.name().to_snake_case();

        CallbackInterfaceVTable {
            type_: cpp_type(&cbi.vtable()),
            var_name: format!("kCallbackInterfaceVtable{cbi_name}"),
            method_handlers: cbi
                .vtable_methods()
                .iter()
                .map(|(_, method)| {
                    let method_name = method.name().to_upper_camel_case();
                    let method_name_snake = method.name().to_snake_case();
                    CallbackMethodHandler {
                        fn_name: format!("callback_interface_{cbi_name_snake}_{method_name_snake}"),
                        class_name: format!("CallbackInterfaceMethod{cbi_name}{method_name}"),
                        arguments: method
                            .arguments()
                            .iter()
                            .map(|arg| CallbackMethodArgument {
                                name: arg.name().to_snake_case(),
                                type_: cpp_type(&arg.as_type().into()),
                                scaffolding_converter: scaffolding_converter(
                                    ci,
                                    &arg.as_type().into(),
                                ),
                            })
                            .collect(),
                    }
                })
                .collect(),
        }
    }

    fn scaffolding_calls(
        function_ids: &FunctionIds<'_>,
        components: &[Component],
    ) -> Vec<ScaffoldingCall> {
        let mut calls: Vec<ScaffoldingCall> = components
            .iter()
            .flat_map(|c| {
                exposed_functions(&c.ci).map(move |(callable, ffi_func)| {
                    ScaffoldingCall::new(&c.ci, callable, ffi_func, function_ids)
                })
            })
            .collect();
        calls.sort_by_key(|c| c.function_id);
        calls
    }
}

/// Combines fixture and non-fixture template items
struct CombinedItems<T> {
    item: T,
    fixture_item: T,
}

impl<T> CombinedItems<T> {
    fn new(item: T, fixture_item: T) -> Self {
        Self { item, fixture_item }
    }

    /// Iterate over child items
    /// Each item is the tuple (preprocssor_condition, <T>, preprocssor_condition_end), where
    /// `preprocssor_condition` is the preprocessor preprocssor_condition that should control if
    /// the items are included.
    fn iter(&self) -> impl Iterator<Item = (String, &T, String)> {
        vec![
            ("".to_string(), &self.item, "".to_string()),
            (
                "#ifdef MOZ_UNIFFI_FIXTURES".to_string(),
                &self.fixture_item,
                "#endif /* MOZ_UNIFFI_FIXTURES */".to_string(),
            ),
        ]
        .into_iter()
    }
}

enum FfiDefinitionCpp {
    Function(FfiFunctionCpp),
    CallbackFunction(FfiCallbackFunctionCpp),
    Struct(FfiStructCpp),
}

impl FfiDefinitionCpp {
    fn name(&self) -> &str {
        match self {
            Self::Function(f) => &f.name,
            Self::CallbackFunction(c) => &c.name,
            Self::Struct(s) => &s.name,
        }
    }
}

struct FfiFunctionCpp {
    name: String,
    arg_types: Vec<String>,
    return_type: String,
}

struct FfiCallbackFunctionCpp {
    name: String,
    arg_types: Vec<String>,
    return_type: String,
}

struct FfiStructCpp {
    name: String,
    fields: Vec<FfiFieldCpp>,
}

struct FfiFieldCpp {
    name: String,
    type_: String,
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
    /// Static variable that stores a reference to the JS UniFFICallbackHandler object
    js_handler_var: String,
    vtable: CallbackInterfaceVTable,
    free_fn: String,
    init_fn: String,
}

/// Represents the vtable for a callback interface
///
/// "vtable" just means a struct whose fields are function pointers -- one for each method.
struct CallbackInterfaceVTable {
    /// FFI struct name
    type_: String,
    /// Name of the static variable storing the vtable
    var_name: String,
    /// Functions to handle the callback interface methods
    ///
    /// These are then stored in the vtable fields
    method_handlers: Vec<CallbackMethodHandler>,
}

/// Code to handle a single callback interface method
struct CallbackMethodHandler {
    /// C++ function to handle the method
    fn_name: String,
    /// UniffiCallbackMethodHandlerBase subclass for this method
    class_name: String,
    arguments: Vec<CallbackMethodArgument>,
}

struct CallbackMethodArgument {
    name: String,
    type_: String,
    scaffolding_converter: String,
}

struct ScaffoldingCall {
    handler_class_name: String,
    function_id: usize,
    ffi_func_name: String,
    return_type: Option<ScaffoldingCallReturnType>,
    arguments: Vec<ScaffoldingCallArgument>,
    async_info: Option<ScaffoldingCallAsyncInfo>,
}

impl ScaffoldingCall {
    fn new(
        ci: &ComponentInterface,
        callable: &dyn Callable,
        ffi_func: &FfiFunction,
        function_ids: &FunctionIds,
    ) -> Self {
        let handler_class_name = format!(
            "ScaffoldingCallHandler{}",
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

        let async_info = callable.is_async().then(|| ScaffoldingCallAsyncInfo {
            poll_fn: callable.ffi_rust_future_poll(ci),
            complete_fn: callable.ffi_rust_future_complete(ci),
            free_fn: callable.ffi_rust_future_free(ci),
        });

        Self {
            handler_class_name,
            function_id: function_ids.get(ci, ffi_func),
            ffi_func_name: ffi_func.name().to_owned(),
            // Make sure to use the callable here, not the ffi_func.  For async functions, the FFI
            // function always returns a handle.
            return_type: callable
                .return_type()
                .map(|return_type| ScaffoldingCallReturnType {
                    scaffolding_converter: scaffolding_converter(ci, &return_type.into()),
                }),
            arguments,
            async_info,
        }
    }

    fn is_async(&self) -> bool {
        self.async_info.is_some()
    }
}

struct ScaffoldingCallReturnType {
    scaffolding_converter: String,
}

struct ScaffoldingCallArgument {
    var_name: String,
    scaffolding_converter: String,
}

struct ScaffoldingCallAsyncInfo {
    poll_fn: String,
    complete_fn: String,
    free_fn: String,
}

fn scaffolding_converter(ci: &ComponentInterface, ffi_type: &FfiType) -> String {
    match ffi_type {
        FfiType::RustArcPtr(name) => {
            // Check if this is an external type
            for (extern_name, crate_name, _, _) in ci.iter_external_types() {
                if extern_name == name {
                    return format!(
                        "ScaffoldingObjectConverter<&{}>",
                        pointer_type(crate_name_to_namespace(&crate_name), name),
                    );
                }
            }
            format!(
                "ScaffoldingObjectConverter<&{}>",
                pointer_type(ci.namespace(), name),
            )
        }
        _ => format!("ScaffoldingConverter<{}>", cpp_type(ffi_type)),
    }
}

fn pointer_type(namespace: &str, name: &str) -> String {
    format!(
        "k{}{}PointerType",
        namespace.to_upper_camel_case(),
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
        FfiType::ForeignBytes => "ForeignBytes".to_owned(),
        FfiType::Handle => "uint64_t".to_owned(),
        FfiType::RustCallStatus => "RustCallStatus".to_owned(),
        FfiType::Callback(name) | FfiType::Struct(name) => name.to_owned(),
        FfiType::VoidPointer => "void*".to_owned(),
        FfiType::Reference(inner) => format!("{}*", cpp_type(inner.as_ref())),
    }
}

fn return_type(ffi_type: Option<&FfiType>) -> String {
    match ffi_type {
        Some(t) => cpp_type(t),
        None => "void".to_owned(),
    }
}

// Iterate over functions, methods, and constructors exposed to JS
//
// Generates `&dyn Callable` items, since of these is a different type, but they all implement
// `Callable`.
//
// Also generates `&FfiFunction` for each item.  There should probably be a method on `Callable`
// that returns this and there's a PR to do so, but in the meantime we need to use this workaround.
pub fn exposed_functions(
    ci: &ComponentInterface,
) -> impl Iterator<Item = (&dyn Callable, &FfiFunction)> {
    ci.function_definitions()
        .into_iter()
        .map(|f| (f as &dyn Callable, f.ffi_func()))
        .chain(ci.object_definitions().into_iter().flat_map(|o| {
            o.methods()
                .into_iter()
                .map(|m| (m as &dyn Callable, m.ffi_func()))
                .chain(
                    o.constructors()
                        .into_iter()
                        .map(|c| (c as &dyn Callable, c.ffi_func())),
                )
        }))
}
