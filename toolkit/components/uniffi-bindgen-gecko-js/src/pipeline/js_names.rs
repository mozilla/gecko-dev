/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;
use heck::{ToLowerCamelCase, ToShoutySnakeCase, ToUpperCamelCase};

pub fn pass(root: &mut Root) -> Result<()> {
    root.visit_mut(|func: &mut Function| func.name = func.name.to_lower_camel_case());
    root.visit_mut(|meth: &mut Method| meth.name = meth.name.to_lower_camel_case());
    root.visit_mut(|cons: &mut Constructor| cons.name = cons.name.to_lower_camel_case());
    root.visit_mut(|callable: &mut Callable| callable.name = callable.name.to_lower_camel_case());

    root.visit_mut(|rec: &mut Record| rec.name = rec.name.to_upper_camel_case());
    root.visit_mut(|en: &mut Enum| {
        en.name = en.name.to_upper_camel_case();
        if en.is_flat && !en.self_type.is_used_as_error {
            en.visit_mut(|v: &mut Variant| v.name = v.name.to_shouty_snake_case());
        } else {
            en.visit_mut(|v: &mut Variant| v.name = v.name.to_upper_camel_case());
        }
    });
    root.visit_mut(|int: &mut Interface| int.name = int.name.to_upper_camel_case());
    root.visit_mut(|cbi: &mut CustomType| cbi.name = cbi.name.to_upper_camel_case());
    root.visit_mut(|custom: &mut CustomType| custom.name = custom.name.to_upper_camel_case());
    root.visit_mut(|arg: &mut Argument| arg.name = arg.name.to_lower_camel_case());
    root.visit_mut(|field: &mut Field| field.name = field.name.to_lower_camel_case());
    root.visit_mut(|module: &mut Module| module.js_name = format_module_name(&module.name));
    root.visit_mut(|ty: &mut Type| match ty {
        Type::Record {
            module_name, name, ..
        }
        | Type::Enum {
            module_name, name, ..
        }
        | Type::Interface {
            module_name, name, ..
        }
        | Type::CallbackInterface {
            module_name, name, ..
        }
        | Type::Custom {
            module_name, name, ..
        } => {
            *module_name = format_module_name(module_name);
            *name = name.to_upper_camel_case();
        }
        _ => (),
    });
    root.visit_mut(|ext: &mut ExternalType| {
        ext.module_name = format_module_name(&ext.module_name);
        ext.name = ext.name.to_upper_camel_case();
    });
    Ok(())
}

fn format_module_name(source_name: &str) -> String {
    format!("Rust{}", source_name.to_upper_camel_case())
}
