/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::shared::{FunctionExt, ObjectExt};
use crate::{CallbackIds, Config, FunctionIds, ObjectIds};
use askama::Template;
use extend::ext;
use heck::{ToShoutySnakeCase, ToUpperCamelCase};
use std::collections::HashSet;
use std::iter;
use uniffi_bindgen::interface::{
    CallbackInterface, ComponentInterface, FfiArgument, FfiFunction, FfiType, Object,
};

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
    components: &'a Vec<(ComponentInterface, Config)>,
    object_ids: &'a ObjectIds<'a>,
    callback_ids: &'a CallbackIds<'a>,
    has_any_objects: bool,
    /// Info for each scaffolding call.
    scaffolding_calls: Vec<ScaffoldingCall>,
}

impl<'a> CPPScaffoldingTemplate<'a> {
    pub fn new(
        prefix: &'a str,
        components: &'a Vec<(ComponentInterface, Config)>,
        function_ids: &'a FunctionIds<'a>,
        object_ids: &'a ObjectIds<'a>,
        callback_ids: &'a CallbackIds<'a>,
    ) -> Self {
        let has_any_objects = components
            .iter()
            .any(|(ci, _)| ci.object_definitions().len() > 0);
        Self {
            prefix,
            components,
            object_ids,
            callback_ids,
            has_any_objects,
            scaffolding_calls: Self::scaffolding_calls(prefix, components, function_ids),
        }
    }

    fn scaffolding_calls(
        prefix: &str,
        components: &[(ComponentInterface, Config)],
        function_ids: &'a FunctionIds<'a>,
    ) -> Vec<ScaffoldingCall> {
        let mut calls: Vec<ScaffoldingCall> = components
            .iter()
            .flat_map(|(ci, config)| {
                ci.scaffolding_call_functions(config)
                    .into_iter()
                    .map(|(ffi_func, _)| ScaffoldingCall::new(prefix, ci, ffi_func, function_ids))
                    .collect::<Vec<_>>()
            })
            .collect();
        calls.sort_by_key(|c| c.function_id);
        calls
    }
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
                scaffolding_converter: ci.scaffolding_converter(&a.type_()),
            })
            .collect::<Vec<_>>();

        Self {
            handler_class_name,
            function_id: function_ids.get(ci, ffi_func),
            ffi_func_name: ffi_func.name().to_owned(),
            return_type: ffi_func
                .return_type()
                .map(|return_type| ScaffoldingCallReturnType {
                    scaffolding_converter: ci.scaffolding_converter(&return_type),
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

// Define extension traits with methods used in our template code

#[ext(name=ComponentInterfaceCppExt)]
pub impl ComponentInterface {
    // C++ pointer type name.  This needs to be a valid C++ type name and unique across all UDL
    // files.
    fn pointer_type(&self, object: &Object) -> String {
        self._pointer_type(object.name())
    }

    fn _pointer_type(&self, name: &str) -> String {
        format!(
            "k{}{}PointerType",
            self.namespace().to_upper_camel_case(),
            name.to_upper_camel_case()
        )
    }

    // Iterate over all functions to expose via the UniFFIScaffolding class
    //
    // This is basically all the user functions, except we don't expose the free methods for
    // objects.  Freeing is handled by the UniFFIPointer class.
    //
    // Note: this function should return `impl Iterator<&FfiFunction>`, but that's not currently
    // allowed for traits.
    fn exposed_functions(&self) -> Vec<&FfiFunction> {
        let excluded: HashSet<_> = self
            .object_definitions()
            .iter()
            .map(|o| o.ffi_object_free().name())
            .chain(
                self.callback_interface_definitions()
                    .iter()
                    .map(|cbi| cbi.ffi_init_callback().name()),
            )
            .collect();
        self.iter_user_ffi_function_definitions()
            .filter(move |f| !excluded.contains(f.name()))
            .collect()
    }

    // Generate scaffolding call functions used in the interface
    //
    // This is used to generate the `uniffiScaffoldingCall*` classes that implement these calls.
    //
    // Generates both the FfiFunction and also if the function is "JS-async", i.e. should we
    // generate code to dispatch the call to a worker thread.
    fn scaffolding_call_functions(&self, config: &Config) -> Vec<(&FfiFunction, bool)> {
        self.function_definitions()
            .into_iter()
            .map(|f| (f.ffi_func(), f.is_js_async(config)))
            .chain(self.object_definitions().into_iter().flat_map(|o| {
                o.methods()
                    .into_iter()
                    .map(|m| (m.ffi_func(), o.is_method_async(m, config)))
                    .chain(
                        o.constructors()
                            .into_iter()
                            .map(|c| (c.ffi_func(), o.is_constructor_async(config))),
                    )
                    .collect::<Vec<_>>()
            }))
            .collect()
    }

    // ScaffoldingConverter class
    //
    // This is used to convert types between the JS code and Rust
    fn scaffolding_converter(&self, ffi_type: &FfiType) -> String {
        match ffi_type {
            FfiType::RustArcPtr(name) => {
                format!("ScaffoldingObjectConverter<&{}>", self._pointer_type(name),)
            }
            _ => format!("ScaffoldingConverter<{}>", ffi_type.cpp_type()),
        }
    }

    // ScaffoldingCallHandler class
    fn scaffolding_call_handler(&self, func: &FfiFunction) -> String {
        let return_param = match func.return_type() {
            Some(return_type) => self.scaffolding_converter(return_type),
            None => "ScaffoldingConverter<void>".to_string(),
        };
        let all_params = iter::once(return_param)
            .chain(
                func.arguments()
                    .into_iter()
                    .map(|a| self.scaffolding_converter(&a.type_())),
            )
            .collect::<Vec<_>>()
            .join(", ");
        return format!("ScaffoldingCallHandler<{}>", all_params);
    }
}

#[ext(name=FFIFunctionCppExt)]
pub impl FfiFunction {
    fn js_name(&self) -> String {
        self.name().to_upper_camel_case()
    }

    fn cpp_name(&self) -> String {
        self.name().to_string()
    }

    fn cpp_return_type(&self) -> String {
        match self.return_type() {
            Some(t) => t.cpp_type(),
            None => "void".to_owned(),
        }
    }

    fn cpp_arg_list(&self) -> String {
        let mut parts: Vec<String> = self.arguments().iter().map(|a| a.cpp_type()).collect();
        parts.push("RustCallStatus*".to_owned());
        parts.join(", ")
    }
}

#[ext(name=FFITypeCppExt)]
pub impl FfiType {
    // Type for the Rust scaffolding code
    fn cpp_type(&self) -> String {
        match self {
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
}

#[ext(name=FFIArgumentCppExt)]
pub impl FfiArgument {
    fn cpp_type(&self) -> String {
        self.type_().cpp_type()
    }
}

#[ext(name=ObjectCppExt)]
pub impl Object {
    fn js_name(&self) -> String {
        self.name().to_upper_camel_case()
    }
}

#[ext(name=CallbackInterfaceCppExt)]
pub impl CallbackInterface {
    fn js_name(&self) -> String {
        self.name().to_upper_camel_case()
    }

    /// Name of the static pointer to the JS callback handler
    fn js_handler(&self) -> String {
        format!("JS_CALLBACK_HANDLER_{}", self.name().to_shouty_snake_case())
    }

    /// Name of the C function handler
    fn c_handler(&self, prefix: &str) -> String {
        format!(
            "{prefix}CallbackHandler{}",
            self.name().to_upper_camel_case()
        )
    }
}
