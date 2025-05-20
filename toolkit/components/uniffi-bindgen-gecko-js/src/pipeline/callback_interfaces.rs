/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use heck::ToUpperCamelCase;

use super::*;

pub fn pass(module: &mut Module) -> Result<()> {
    let mut saw_callback_interface = false;
    module.visit_mut(|cbi: &mut CallbackInterface| {
        cbi.js_handler_var = format!("uniffiCallbackHandler{}", cbi.name.to_upper_camel_case());
        saw_callback_interface = true;
    });
    module.has_callback_interface = saw_callback_interface;
    Ok(())
}
