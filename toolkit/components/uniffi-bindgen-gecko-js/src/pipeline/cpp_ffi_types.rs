/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;
use anyhow::bail;
use heck::ToUpperCamelCase;

pub fn pass(root: &mut Root) -> Result<()> {
    root.visit_mut(|node: &mut FfiTypeNode| {
        node.type_name = ffi_type_name(&node.ty);
    });
    root.visit_mut(|return_ty: &mut FfiReturnType| {
        return_ty.type_name = match &return_ty.ty {
            Some(type_node) => type_node.type_name.clone(),
            None => "void".to_string(),
        };
    });
    root.try_visit_mut(|arg: &mut FfiValueArgument| {
        arg.ffi_value_class = ffi_value_class(&arg.ty)?;
        Ok(())
    })?;
    root.try_visit_mut(|return_ty: &mut FfiValueReturnType| {
        return_ty.ffi_value_class = ffi_value_class(&return_ty.ty)?;
        Ok(())
    })?;

    Ok(())
}

fn ffi_type_name(ty: &FfiType) -> String {
    match ty {
        FfiType::UInt8 => "uint8_t".to_owned(),
        FfiType::Int8 => "int8_t".to_owned(),
        FfiType::UInt16 => "uint16_t".to_owned(),
        FfiType::Int16 => "int16_t".to_owned(),
        FfiType::UInt32 => "uint32_t".to_owned(),
        FfiType::Int32 => "int32_t".to_owned(),
        FfiType::UInt64 => "uint64_t".to_owned(),
        FfiType::Int64 => "int64_t".to_owned(),
        FfiType::Float32 => "float".to_owned(),
        FfiType::Float64 => "double".to_owned(),
        FfiType::RustBuffer(_) => "RustBuffer".to_owned(),
        FfiType::RustArcPtr { .. } => "void*".to_owned(),
        FfiType::ForeignBytes => "ForeignBytes".to_owned(),
        FfiType::Handle(_) => "uint64_t".to_owned(),
        FfiType::RustCallStatus => "RustCallStatus".to_owned(),
        FfiType::Function(name) => name.0.to_owned(),
        FfiType::Struct(name) => name.0.to_owned(),
        FfiType::VoidPointer => "void*".to_owned(),
        FfiType::MutReference(inner) | FfiType::Reference(inner) => {
            format!("{}*", ffi_type_name(inner.as_ref()))
        }
    }
}

pub fn ffi_value_class(node: &FfiTypeNode) -> Result<String> {
    Ok(match &node.ty {
        FfiType::RustArcPtr {
            module_name,
            object_name,
        } => {
            format!(
                "FfiValueObjectHandle{}{}",
                module_name.to_upper_camel_case(),
                object_name.to_upper_camel_case(),
            )
        }
        FfiType::UInt8
        | FfiType::Int8
        | FfiType::UInt16
        | FfiType::Int16
        | FfiType::UInt32
        | FfiType::Int32
        | FfiType::UInt64
        | FfiType::Int64 => format!("FfiValueInt<{}>", node.type_name),
        FfiType::Float32 | FfiType::Float64 => {
            format!("FfiValueFloat<{}>", node.type_name)
        }
        FfiType::RustBuffer(_) => "FfiValueRustBuffer".to_owned(),
        FfiType::Handle(HandleKind::CallbackInterface { .. }) => "FfiValueInt<uint64_t>".to_owned(),
        ty => bail!("No FfiValue class for: {ty:?}"),
    })
}
