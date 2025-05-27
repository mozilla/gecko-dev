/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;

use anyhow::anyhow;
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
