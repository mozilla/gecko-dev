/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;

pub fn pass(en: &mut Enum) -> Result<()> {
    // Determine the actual discriminants for each variant.
    //
    // TODO: the UniFFI general pipeline should probably do this.  If this was introduced earlier
    // in the pipeline, we wouldn't need to hand-code so many fields.
    en.resolved_discr_type = match &en.discr_type {
        Some(type_node) => type_node.clone(),
        None => TypeNode {
            ty: Type::UInt8,
            canonical_name: "UInt8".to_string(),
            ffi_converter: "FfiConverterUInt8".to_string(),
            is_used_as_error: false,
            ffi_type: FfiTypeNode {
                ty: FfiType::UInt8,
                type_name: "uint8_t".to_string(),
            },
            ..TypeNode::default()
        },
    };

    let discr_type = &en.resolved_discr_type;
    let mut last_variant: Option<&Variant> = None;
    for variant in en.variants.iter_mut() {
        match &variant.discr {
            None => {
                let lit = match last_variant {
                    None => match &discr_type.ty {
                        Type::UInt8 | Type::UInt16 | Type::UInt32 | Type::UInt64 => {
                            Literal::UInt(0, Radix::Decimal, discr_type.clone())
                        }
                        Type::Int8 | Type::Int16 | Type::Int32 | Type::Int64 => {
                            Literal::Int(0, Radix::Decimal, discr_type.clone())
                        }
                        ty => bail!("Invalid enum discriminant type: {ty:?}"),
                    },
                    Some(variant) => match &variant.resolved_discr.lit {
                        Literal::UInt(val, _, _) => {
                            Literal::UInt(val + 1, Radix::Decimal, discr_type.clone())
                        }
                        Literal::Int(val, _, _) => {
                            Literal::Int(val + 1, Radix::Decimal, discr_type.clone())
                        }
                        lit => bail!("Invalid enum discriminant literal: {lit:?}"),
                    },
                };
                variant.resolved_discr = LiteralNode {
                    lit,
                    ..LiteralNode::default()
                };
            }
            Some(lit_node) => {
                variant.resolved_discr = lit_node.clone();
            }
        }
        last_variant = Some(variant);
    }
    Ok(())
}
