/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::anyhow;
use std::collections::HashMap;

use super::*;
use heck::ToUpperCamelCase;

pub fn pass(root: &mut Root) -> Result<()> {
    // Generate `ScaffoldingCall` items
    root.cpp_scaffolding.scaffolding_calls = CombinedItems::try_new(root, |module, ids, items| {
        let module_name = module.name.clone();
        let mut ffi_func_map = HashMap::new();
        module.visit(|ffi_func: &FfiFunction| {
            if ffi_func.kind == FfiFunctionKind::Scaffolding {
                ffi_func_map.insert(ffi_func.name.0.clone(), ffi_func.clone());
            }
        });
        module.try_visit_mut(|callable: &mut Callable| {
            // Callback interface methods don't have scaffolding functions associated with them
            if matches!(callable.kind, CallableKind::VTableMethod { .. }) {
                return Ok(());
            }
            let ffi_func = ffi_func_map
                .remove(&callable.ffi_func.0)
                .ok_or_else(|| anyhow!("FfiFunction lookup failed: {:?}", callable.ffi_func.0))?;
            callable.id = ids.new_id();
            let mut arguments = match &callable.kind {
                CallableKind::Method { interface_name, .. } => vec![FfiValueArgument {
                    name: "uniffi_ptr".to_string(),
                    ty: FfiTypeNode {
                        ty: FfiType::RustArcPtr {
                            module_name: module_name.clone(),
                            object_name: interface_name.clone(),
                        },
                        ..FfiTypeNode::default()
                    },
                    ..FfiValueArgument::default()
                }],
                _ => vec![],
            };
            arguments.extend(callable.arguments.iter().map(|a| FfiValueArgument {
                name: a.name.clone(),
                ty: a.ty.ffi_type.clone(),
                ..FfiValueArgument::default()
            }));

            items.push(ScaffoldingCall {
                id: callable.id,
                arguments,
                handler_class_name: format!(
                    "ScaffoldingCallHandler{}",
                    ffi_func.name.0.to_upper_camel_case(),
                ),
                return_ty: callable
                    .return_type
                    .ty
                    .as_ref()
                    .map(|ty| FfiValueReturnType {
                        ty: ty.ffi_type.clone(),
                        ..FfiValueReturnType::default()
                    }),
                ffi_func,
            });
            Ok(())
        })
    })?;
    root.cpp_scaffolding
        .scaffolding_calls
        .sort_by_key(|call| call.id);

    Ok(())
}
