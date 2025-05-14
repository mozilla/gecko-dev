/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;
use heck::{ToShoutySnakeCase, ToUpperCamelCase};

pub fn pass(root: &mut Root) -> Result<()> {
    root.visit_mut(|node: &mut LiteralNode| node.js_lit = js_lit(&node.lit));
    Ok(())
}

fn js_lit(lit: &Literal) -> String {
    match lit {
        Literal::Boolean(inner) => inner.to_string(),
        Literal::String(inner) => format!("\"{}\"", inner),
        Literal::UInt(num, radix, _) => number_lit(radix, num).to_string(),
        Literal::Int(num, radix, _) => number_lit(radix, num).to_string(),
        Literal::Float(num, _) => num.clone(),
        Literal::Enum(name, typ) => enum_lit(&typ.ty, name),
        Literal::EmptyMap => "{}".to_string(),
        Literal::EmptySequence => "[]".to_string(),
        Literal::Some { inner } => js_lit(inner),
        Literal::None => "null".to_string(),
    }
}

fn number_lit(
    radix: &Radix,
    num: impl std::fmt::Display + std::fmt::LowerHex + std::fmt::Octal,
) -> String {
    match radix {
        Radix::Decimal => format!("{}", num),
        Radix::Hexadecimal => format!("{:#x}", num),
        Radix::Octal => format!("{:#o}", num),
    }
}

fn enum_lit(typ: &Type, variant_name: &str) -> String {
    if let Type::Enum { name, .. } = typ {
        format!(
            "{}.{}",
            name.to_upper_camel_case(),
            variant_name.to_shouty_snake_case()
        )
    } else {
        panic!("Rendering an enum literal on a type that is not an enum")
    }
}
