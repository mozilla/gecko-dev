/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::{HashMap, HashSet};

use anyhow::{anyhow, bail};
use heck::{ToSnakeCase, ToUpperCamelCase};

use super::*;

pub fn pass(root: &mut Root) -> Result<()> {
    root.cpp_scaffolding.callback_interfaces =
        CombinedItems::try_new(root, |module, ids, items| {
            let module_name = module.name.clone();
            let ffi_func_map: HashMap<String, FfiFunctionType> = module
                .ffi_definitions
                .iter()
                .filter_map(|def| match def {
                    FfiDefinition::FunctionType(func_type) => Some(func_type),
                    _ => None,
                })
                .map(|func_type| (func_type.name.0.clone(), func_type.clone()))
                .collect();

            module.try_visit_mut(|cbi: &mut CallbackInterface| {
                let interface_name = cbi.name.clone();
                cbi.id = ids.new_id();
                items.push(CppCallbackInterface {
                    id: cbi.id,
                    name: cbi.name.clone(),
                    handler_var: format!(
                        "gUniffiCallbackHandler{}",
                        cbi.name.to_upper_camel_case()
                    ),
                    vtable_var: format!("kUniffiVtable{}", cbi.name.to_upper_camel_case()),
                    vtable_struct_type: cbi.vtable.struct_type.clone(),
                    init_fn: cbi.vtable.init_fn.clone(),
                    free_fn: format!(
                        "callback_free_{}_{}",
                        module_name.to_snake_case(),
                        interface_name.to_snake_case()
                    ),
                    methods: cbi
                        .vtable
                        .methods
                        .iter()
                        .enumerate()
                        .map(|(i, vtable_meth)| {
                            let ffi_func = ffi_func_map
                                .get(&format!("CallbackInterface{interface_name}Method{i}"))
                                .cloned()
                                .ok_or_else(|| {
                                    anyhow!(
                                        "Callback interface method not found: {}",
                                        vtable_meth.callable.name
                                    )
                                })?;
                            map_method(vtable_meth, ffi_func, &module_name, &interface_name)
                        })
                        .collect::<Result<Vec<_>>>()?,
                });
                Ok(())
            })?;
            Ok(())
        })?;

    let mut seen_async_complete_handlers = HashSet::new();
    root.cpp_scaffolding.async_callback_method_handler_bases = root
        .cpp_scaffolding
        .callback_interfaces
        .try_map(|callback_interfaces| {
            let mut async_callback_method_handler_bases = vec![];
            callback_interfaces.try_visit(|meth: &CppCallbackInterfaceMethod| {
                let Some(async_data) = &meth.async_data else {
                    return Ok(());
                };
                if seen_async_complete_handlers
                    .insert(async_data.callback_handler_base_class.clone())
                {
                    async_callback_method_handler_bases.push(AsyncCallbackMethodHandlerBase {
                        class_name: async_data.callback_handler_base_class.clone(),
                        complete_callback_type_name: async_data.complete_callback_type_name.clone(),
                        result_type_name: async_data.result_type_name.clone(),
                        return_type: meth.return_ty.clone(),
                    });
                };
                Ok(())
            })?;
            Ok(async_callback_method_handler_bases)
        })?;
    Ok(())
}

fn map_method(
    meth: &VTableMethod,
    ffi_func: FfiFunctionType,
    module_name: &str,
    interface_name: &str,
) -> Result<CppCallbackInterfaceMethod> {
    let (return_ty, out_pointer_ty) = match &meth.callable.return_type.ty {
        Some(ty) => (
            Some(FfiValueReturnType {
                ty: ty.ffi_type.clone(),
                ..FfiValueReturnType::default()
            }),
            FfiType::MutReference(Box::new(ty.ffi_type.ty.clone())),
        ),
        None => (None, FfiType::VoidPointer),
    };
    Ok(CppCallbackInterfaceMethod {
        async_data: meth
            .callable
            .async_data
            .as_ref()
            .map(|async_data| {
                anyhow::Ok(CppCallbackInterfaceMethodAsyncData {
                    callback_handler_base_class: async_callback_method_handler_base_class(
                        &meth.callable,
                    )?,
                    complete_callback_type_name: async_data.ffi_foreign_future_complete.0.clone(),
                    result_type_name: async_data.ffi_foreign_future_result.0.clone(),
                })
            })
            .transpose()?,
        arguments: meth
            .callable
            .arguments
            .iter()
            .map(|a| FfiValueArgument {
                name: a.name.clone(),
                ty: a.ty.ffi_type.clone(),
                ..FfiValueArgument::default()
            })
            .collect(),
        fn_name: format!(
            "callback_interface_{}_{}_{}",
            module_name.to_snake_case(),
            interface_name.to_snake_case(),
            meth.callable.name.to_snake_case(),
        ),
        base_class_name: match &meth.callable.async_data {
            // Note: non-async callbacks are currently always treated as fire-and-forget methods.
            // These fire-and-forget methods inherit from `AsyncCallbackMethodHandlerBase` directly
            // and have a no-op `HandleReturn` method.
            None => "AsyncCallbackMethodHandlerBase".to_string(),
            Some(_) => async_callback_method_handler_base_class(&meth.callable)?,
        },
        handler_class_name: format!(
            "CallbackInterfaceMethod{}{}{}",
            module_name.to_upper_camel_case(),
            interface_name.to_upper_camel_case(),
            meth.callable.name.to_upper_camel_case(),
        ),
        return_ty,
        out_pointer_ty: FfiTypeNode {
            ty: out_pointer_ty,
            ..FfiTypeNode::default()
        },
        ffi_func,
    })
}

fn async_callback_method_handler_base_class(callable: &Callable) -> Result<String> {
    let return_type = callable.return_type.ty.as_ref().map(|ty| &ty.ffi_type.ty);
    Ok(match return_type {
        None => "AsyncCallbackMethodHandlerBaseVoid".to_string(),
        Some(return_type) => match return_type {
            FfiType::RustArcPtr {
                module_name,
                object_name,
            } => {
                format!(
                    "AsyncCallbackMethodHandlerBaseRustArcPtr{}_{}",
                    module_name.to_upper_camel_case(),
                    object_name.to_upper_camel_case()
                )
            }
            FfiType::UInt8 => "AsyncCallbackMethodHandlerBaseUInt8".to_string(),
            FfiType::Int8 => "AsyncCallbackMethodHandlerBaseInt8".to_string(),
            FfiType::UInt16 => "AsyncCallbackMethodHandlerBaseUInt16".to_string(),
            FfiType::Int16 => "AsyncCallbackMethodHandlerBaseInt16".to_string(),
            FfiType::UInt32 => "AsyncCallbackMethodHandlerBaseUInt32".to_string(),
            FfiType::Int32 => "AsyncCallbackMethodHandlerBaseInt32".to_string(),
            FfiType::UInt64 => "AsyncCallbackMethodHandlerBaseUInt64".to_string(),
            FfiType::Int64 => "AsyncCallbackMethodHandlerBaseInt64".to_string(),
            FfiType::Float32 => "AsyncCallbackMethodHandlerBaseFloat32".to_string(),
            FfiType::Float64 => "AsyncCallbackMethodHandlerBaseFloat64".to_string(),
            FfiType::RustBuffer(_) => "AsyncCallbackMethodHandlerBaseRustBuffer".to_string(),
            ty => bail!("Async return type not supported: {ty:?}"),
        },
    })
}
