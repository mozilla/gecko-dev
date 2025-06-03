/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;
use heck::ToUpperCamelCase;

pub fn pass(root: &mut Root) -> Result<()> {
    root.visit_mut(|node: &mut TypeNode| {
        node.ffi_converter = format!("FfiConverter{}", node.canonical_name);
        node.class_name = class_name(&node.ty);
        node.jsdoc_name = jsdoc_name(&node.ty);
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

fn jsdoc_name(ty: &Type) -> String {
    match ty {
        Type::Int8
        | Type::UInt8
        | Type::Int16
        | Type::UInt16
        | Type::Int32
        | Type::UInt32
        | Type::Int64
        | Type::UInt64
        | Type::Float32
        | Type::Float64 => "number".into(),
        Type::String => "string".into(),
        // TODO: should be Uint8Array
        Type::Bytes => "string".into(),
        Type::Boolean => "boolean".into(),
        Type::Interface { name, .. }
        | Type::Enum { name, .. }
        | Type::Record { name, .. }
        | Type::CallbackInterface { name, .. }
        | Type::Custom { name, .. } => name.to_upper_camel_case(),
        Type::Optional { inner_type } => format!("?{}", jsdoc_name(inner_type)),
        Type::Sequence { inner_type } => format!("Array.<{}>", jsdoc_name(inner_type)),
        Type::Map { .. } => "object".into(),
        Type::Timestamp => unimplemented!("Timestamp"),
        Type::Duration => unimplemented!("Duration"),
    }
}
