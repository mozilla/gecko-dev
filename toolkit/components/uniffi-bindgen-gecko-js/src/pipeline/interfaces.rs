/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Generate the `ScaffoldingCall` lists

use super::*;
use heck::{ToSnakeCase, ToUpperCamelCase};

pub fn pass(root: &mut Root) -> Result<()> {
    root.visit_mut(|int: &mut Interface| {
        let int_ffi_converter = format!("FfiConverter{}", int.self_type.canonical_name);
        int.visit_mut(|callable_kind: &mut CallableKind| {
            if let CallableKind::Method { ffi_converter, .. } = callable_kind {
                *ffi_converter = int_ffi_converter.clone();
            };
        });
        // Rename the primary constructor to `init`
        int.visit_mut(|cons: &mut Constructor| {
            if cons.name == "new" {
                cons.name = "init".to_string();
                cons.visit_mut(|callable: &mut Callable| {
                    callable.name = "init".to_string();
                })
            }
        });
        if let Some(vtable) = &mut int.vtable {
            vtable.interface_name = int.name.clone();
        }
    });

    // Generate [CppScaffolding::pointer_types]
    root.cpp_scaffolding.pointer_types = CombinedItems::new(root, |module, ids, items| {
        let module_name = module.name.clone();
        module.visit_mut(|int: &mut Interface| {
            int.object_id = ids.new_id();
            items.push(PointerType {
                id: int.object_id,
                name: format!(
                    "k{}{}PointerType",
                    module_name.to_upper_camel_case(),
                    int.name.to_upper_camel_case()
                ),
                ffi_value_class: format!(
                    "FfiValueObjectHandle{}{}",
                    module_name.to_upper_camel_case(),
                    int.name.to_upper_camel_case(),
                ),
                label: format!("{}::{}", module_name, int.name),
                ffi_func_clone: int.ffi_func_clone.clone(),
                ffi_func_free: int.ffi_func_free.clone(),
                trait_interface_info: int.vtable.is_some().then(|| PointerTypeTraitInterfaceInfo {
                    free_fn: format!(
                        "callback_free_{}_{}",
                        module_name.to_snake_case(),
                        int.name.to_snake_case(),
                    ),
                }),
            })
        });
    });
    root.cpp_scaffolding
        .pointer_types
        .sort_by_key(|call| call.id);
    Ok(())
}
