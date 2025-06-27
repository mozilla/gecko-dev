/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use heck::ToUpperCamelCase;

use super::*;

pub fn pass(module: &mut Module) -> Result<()> {
    let module_name = module.name.clone();
    module.visit_mut(|cbi: &mut CallbackInterface| {
        cbi.vtable.interface_name = cbi.name.clone();
        cbi.vtable.callback_interface = true;
        cbi.interface_base_class = InterfaceBaseClass {
            name: cbi.name.clone(),
            methods: cbi
                .vtable
                .methods
                .iter()
                .cloned()
                .map(|vtable_meth| {
                    Method {
                        name: vtable_meth.callable.name.clone(),
                        // We don't have docstrings in this case, but that's probably fine
                        docstring: None,
                        callable: vtable_meth.callable,
                        ..Method::default()
                    }
                })
                .collect(),
            docstring: cbi.docstring.clone(),
            ..InterfaceBaseClass::default()
        };
    });
    // Set the js_handler_var for both callback interfaces and trait interfaces
    module.visit_mut(|vtable: &mut VTable| {
        vtable.js_handler_var = format!(
            "uniffiCallbackHandler{}{}",
            module_name.to_upper_camel_case(),
            vtable.interface_name.to_upper_camel_case()
        );
    });
    Ok(())
}
