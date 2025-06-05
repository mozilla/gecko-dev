/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use heck::ToUpperCamelCase;

use super::*;

pub fn pass(module: &mut Module) -> Result<()> {
    module.visit_mut(|cbi: &mut CallbackInterface| {
        cbi.vtable.interface_name = cbi.name.clone();
        cbi.vtable.callback_interface = true;
    });
    // Set the js_handler_var for both callback interfaces and trait interfaces
    module.visit_mut(|vtable: &mut VTable| {
        vtable.js_handler_var = format!(
            "uniffiCallbackHandler{}",
            vtable.interface_name.to_upper_camel_case()
        );
    });
    Ok(())
}
