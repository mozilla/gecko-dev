/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;
use heck::ToUpperCamelCase;

pub fn pass(module: &mut Module) -> Result<()> {
    module.js_filename = format!("Rust{}.sys.mjs", module.name.to_upper_camel_case());
    Ok(())
}
