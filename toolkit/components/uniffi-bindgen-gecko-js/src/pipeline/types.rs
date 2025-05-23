/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;

pub fn pass(root: &mut Root) -> Result<()> {
    root.visit_mut(|node: &mut TypeNode| {
        node.ffi_converter = format!("FfiConverter{}", node.canonical_name);
    });

    Ok(())
}
