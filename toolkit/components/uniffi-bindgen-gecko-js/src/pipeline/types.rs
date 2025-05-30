/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;

pub fn pass(root: &mut Root) -> Result<()> {
    root.visit_mut(|node: &mut TypeNode| {
        node.ffi_converter = format!("FfiConverter{}", node.canonical_name);
        node.class_name = class_name(&node.ty);
    });

    Ok(())
}

fn class_name(ty: &Type) -> Option<String> {
    match ty {
        Type::Interface { name, .. }
        | Type::Record { name, .. }
        | Type::Enum { name, .. }
        | Type::CallbackInterface { name, .. }
        | Type::Custom { name, .. } => Some(name.clone()),
        Type::UInt8
        | Type::Int8
        | Type::UInt16
        | Type::Int16
        | Type::UInt32
        | Type::Int32
        | Type::UInt64
        | Type::Int64
        | Type::Float32
        | Type::Float64
        | Type::Boolean
        | Type::String
        | Type::Bytes
        | Type::Timestamp
        | Type::Duration
        | Type::Sequence { .. }
        | Type::Map { .. } => None,
        Type::Optional { inner_type } => class_name(inner_type),
    }
}
