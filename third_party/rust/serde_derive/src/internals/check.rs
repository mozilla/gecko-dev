// Copyright 2017 Serde Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use internals::ast::{Container, Data, Field, Style};
use internals::attr::{EnumTag, Identifier};
use internals::{Ctxt, Derive};
use syn::{Member, Type};

/// Cross-cutting checks that require looking at more than a single attrs
/// object. Simpler checks should happen when parsing and building the attrs.
pub fn check(cx: &Ctxt, cont: &mut Container, derive: Derive) {
    check_getter(cx, cont);
    check_flatten(cx, cont);
    check_identifier(cx, cont);
    check_variant_skip_attrs(cx, cont);
    check_internal_tag_field_name_conflict(cx, cont);
    check_adjacent_tag_conflict(cx, cont);
    check_transparent(cx, cont, derive);
}

/// Getters are only allowed inside structs (not enums) with the `remote`
/// attribute.
fn check_getter(cx: &Ctxt, cont: &Container) {
    match cont.data {
        Data::Enum(_, _) => {
            if cont.data.has_getter() {
                cx.error("#[serde(getter = \"...\")] is not allowed in an enum");
            }
        }
        Data::Struct(_, _) => {
            if cont.data.has_getter() && cont.attrs.remote().is_none() {
                cx.error(
                    "#[serde(getter = \"...\")] can only be used in structs \
                     that have #[serde(remote = \"...\")]",
                );
            }
        }
    }
}

/// Flattening has some restrictions we can test.
fn check_flatten(cx: &Ctxt, cont: &Container) {
    match cont.data {
        Data::Enum(_, ref variants) => {
            for variant in variants {
                for field in &variant.fields {
                    check_flatten_field(cx, variant.style, field);
                }
            }
        }
        Data::Struct(style, ref fields) => {
            for field in fields {
                check_flatten_field(cx, style, field);
            }
        }
    }
}

fn check_flatten_field(cx: &Ctxt, style: Style, field: &Field) {
    if !field.attrs.flatten() {
        return;
    }
    match style {
        Style::Tuple => {
            cx.error("#[serde(flatten)] cannot be used on tuple structs");
        }
        Style::Newtype => {
            cx.error("#[serde(flatten)] cannot be used on newtype structs");
        }
        _ => {}
    }
    if field.attrs.skip_serializing() {
        cx.error(
            "#[serde(flatten] can not be combined with \
             #[serde(skip_serializing)]",
        );
    } else if field.attrs.skip_serializing_if().is_some() {
        cx.error(
            "#[serde(flatten] can not be combined with \
             #[serde(skip_serializing_if = \"...\")]",
        );
    } else if field.attrs.skip_deserializing() {
        cx.error(
            "#[serde(flatten] can not be combined with \
             #[serde(skip_deserializing)]",
        );
    }
}

/// The `other` attribute must be used at most once and it must be the last
/// variant of an enum.
///
/// Inside a `variant_identifier` all variants must be unit variants. Inside a
/// `field_identifier` all but possibly one variant must be unit variants. The
/// last variant may be a newtype variant which is an implicit "other" case.
fn check_identifier(cx: &Ctxt, cont: &Container) {
    let variants = match cont.data {
        Data::Enum(_, ref variants) => variants,
        Data::Struct(_, _) => {
            return;
        }
    };

    for (i, variant) in variants.iter().enumerate() {
        match (
            variant.style,
            cont.attrs.identifier(),
            variant.attrs.other(),
            cont.attrs.tag(),
        ) {
            // The `other` attribute may not be used in a variant_identifier.
            (_, Identifier::Variant, true, _) => {
                cx.error("#[serde(other)] may not be used on a variant_identifier");
            }

            // Variant with `other` attribute cannot appear in untagged enum
            (_, Identifier::No, true, &EnumTag::None) => {
                cx.error("#[serde(other)] cannot appear on untagged enum");
            }

            // Variant with `other` attribute must be the last one.
            (Style::Unit, Identifier::Field, true, _) | (Style::Unit, Identifier::No, true, _) => {
                if i < variants.len() - 1 {
                    cx.error("#[serde(other)] must be the last variant");
                }
            }

            // Variant with `other` attribute must be a unit variant.
            (_, Identifier::Field, true, _) | (_, Identifier::No, true, _) => {
                cx.error("#[serde(other)] must be on a unit variant");
            }

            // Any sort of variant is allowed if this is not an identifier.
            (_, Identifier::No, false, _) => {}

            // Unit variant without `other` attribute is always fine.
            (Style::Unit, _, false, _) => {}

            // The last field is allowed to be a newtype catch-all.
            (Style::Newtype, Identifier::Field, false, _) => {
                if i < variants.len() - 1 {
                    cx.error(format!("`{}` must be the last variant", variant.ident));
                }
            }

            (_, Identifier::Field, false, _) => {
                cx.error("field_identifier may only contain unit variants");
            }

            (_, Identifier::Variant, false, _) => {
                cx.error("variant_identifier may only contain unit variants");
            }
        }
    }
}

/// Skip-(de)serializing attributes are not allowed on variants marked
/// (de)serialize_with.
fn check_variant_skip_attrs(cx: &Ctxt, cont: &Container) {
    let variants = match cont.data {
        Data::Enum(_, ref variants) => variants,
        Data::Struct(_, _) => {
            return;
        }
    };

    for variant in variants.iter() {
        if variant.attrs.serialize_with().is_some() {
            if variant.attrs.skip_serializing() {
                cx.error(format!(
                    "variant `{}` cannot have both #[serde(serialize_with)] and \
                     #[serde(skip_serializing)]",
                    variant.ident
                ));
            }

            for field in &variant.fields {
                let member = member_message(&field.member);

                if field.attrs.skip_serializing() {
                    cx.error(format!(
                        "variant `{}` cannot have both #[serde(serialize_with)] and \
                         a field {} marked with #[serde(skip_serializing)]",
                        variant.ident, member
                    ));
                }

                if field.attrs.skip_serializing_if().is_some() {
                    cx.error(format!(
                        "variant `{}` cannot have both #[serde(serialize_with)] and \
                         a field {} marked with #[serde(skip_serializing_if)]",
                        variant.ident, member
                    ));
                }
            }
        }

        if variant.attrs.deserialize_with().is_some() {
            if variant.attrs.skip_deserializing() {
                cx.error(format!(
                    "variant `{}` cannot have both #[serde(deserialize_with)] and \
                     #[serde(skip_deserializing)]",
                    variant.ident
                ));
            }

            for field in &variant.fields {
                if field.attrs.skip_deserializing() {
                    let member = member_message(&field.member);

                    cx.error(format!(
                        "variant `{}` cannot have both #[serde(deserialize_with)] \
                         and a field {} marked with #[serde(skip_deserializing)]",
                        variant.ident, member
                    ));
                }
            }
        }
    }
}

/// The tag of an internally-tagged struct variant must not be
/// the same as either one of its fields, as this would result in
/// duplicate keys in the serialized output and/or ambiguity in
/// the to-be-deserialized input.
fn check_internal_tag_field_name_conflict(cx: &Ctxt, cont: &Container) {
    let variants = match cont.data {
        Data::Enum(_, ref variants) => variants,
        Data::Struct(_, _) => return,
    };

    let tag = match *cont.attrs.tag() {
        EnumTag::Internal { ref tag } => tag.as_str(),
        EnumTag::External | EnumTag::Adjacent { .. } | EnumTag::None => return,
    };

    let diagnose_conflict = || {
        let message = format!("variant field name `{}` conflicts with internal tag", tag);
        cx.error(message);
    };

    for variant in variants {
        match variant.style {
            Style::Struct => {
                for field in &variant.fields {
                    let check_ser = !field.attrs.skip_serializing();
                    let check_de = !field.attrs.skip_deserializing();
                    let name = field.attrs.name();
                    let ser_name = name.serialize_name();
                    let de_name = name.deserialize_name();

                    if check_ser && ser_name == tag || check_de && de_name == tag {
                        diagnose_conflict();
                        return;
                    }
                }
            }
            Style::Unit | Style::Newtype | Style::Tuple => {}
        }
    }
}

/// In the case of adjacently-tagged enums, the type and the
/// contents tag must differ, for the same reason.
fn check_adjacent_tag_conflict(cx: &Ctxt, cont: &Container) {
    let (type_tag, content_tag) = match *cont.attrs.tag() {
        EnumTag::Adjacent {
            ref tag,
            ref content,
        } => (tag, content),
        EnumTag::Internal { .. } | EnumTag::External | EnumTag::None => return,
    };

    if type_tag == content_tag {
        let message = format!(
            "enum tags `{}` for type and content conflict with each other",
            type_tag
        );
        cx.error(message);
    }
}

/// Enums and unit structs cannot be transparent.
fn check_transparent(cx: &Ctxt, cont: &mut Container, derive: Derive) {
    if !cont.attrs.transparent() {
        return;
    }

    if cont.attrs.type_from().is_some() {
        cx.error("#[serde(transparent)] is not allowed with #[serde(from = \"...\")]");
    }

    if cont.attrs.type_into().is_some() {
        cx.error("#[serde(transparent)] is not allowed with #[serde(into = \"...\")]");
    }

    let fields = match cont.data {
        Data::Enum(_, _) => {
            cx.error("#[serde(transparent)] is not allowed on an enum");
            return;
        }
        Data::Struct(Style::Unit, _) => {
            cx.error("#[serde(transparent)] is not allowed on a unit struct");
            return;
        }
        Data::Struct(_, ref mut fields) => fields,
    };

    let mut transparent_field = None;

    for field in fields {
        if allow_transparent(field, derive) {
            if transparent_field.is_some() {
                cx.error(
                    "#[serde(transparent)] requires struct to have at most one transparent field",
                );
                return;
            }
            transparent_field = Some(field);
        }
    }

    match transparent_field {
        Some(transparent_field) => transparent_field.attrs.mark_transparent(),
        None => match derive {
            Derive::Serialize => {
                cx.error("#[serde(transparent)] requires at least one field that is not skipped");
            }
            Derive::Deserialize => {
                cx.error("#[serde(transparent)] requires at least one field that is neither skipped nor has a default");
            }
        },
    }
}

fn member_message(member: &Member) -> String {
    match *member {
        Member::Named(ref ident) => format!("`{}`", ident),
        Member::Unnamed(ref i) => i.index.to_string(),
    }
}

fn allow_transparent(field: &Field, derive: Derive) -> bool {
    if let Type::Path(ref ty) = *field.ty {
        if let Some(seg) = ty.path.segments.last() {
            if seg.into_value().ident == "PhantomData" {
                return false;
            }
        }
    }

    match derive {
        Derive::Serialize => !field.attrs.skip_serializing(),
        Derive::Deserialize => !field.attrs.skip_deserializing() && field.attrs.default().is_none(),
    }
}
