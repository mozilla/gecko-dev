// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::fmt::{self, Debug};

const DEFAULT_BYTES_TYPE_NAME: &str = "Bytes";

pub fn pretty_bytes(bytes: &[u8]) -> PrettyBytes<'_> {
    PrettyBytes {
        ty: None,
        bytes,
        show_len: true,
        show_raw: false,
    }
}

pub struct PrettyBytes<'a> {
    ty: Option<&'a str>,
    bytes: &'a [u8],
    show_len: bool,
    show_raw: bool,
}

impl<'a> PrettyBytes<'a> {
    pub fn named(self, ty: &'a str) -> Self {
        Self {
            ty: Some(ty),
            ..self
        }
    }

    pub fn show_len(self, show: bool) -> Self {
        Self {
            show_len: show,
            ..self
        }
    }

    pub fn show_raw(self, show: bool) -> Self {
        Self {
            show_raw: show,
            ..self
        }
    }
}

impl Debug for PrettyBytes<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let show_raw = self.show_raw || f.alternate();
        match (self.ty, self.show_len, show_raw) {
            (_, false, false) => show_only_type(self.ty, f),
            (None, false, true) => show_only_raw(self.bytes, f),
            (Some(ty), false, true) => show_newtype(ty, self.bytes, f),
            (_, true, _) => show_struct(self.ty, self.bytes, show_raw, f),
        }
    }
}

fn show_only_type(ty: Option<&str>, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    f.write_str(ty.unwrap_or(DEFAULT_BYTES_TYPE_NAME))
}

fn show_only_raw(bytes: &[u8], f: &mut fmt::Formatter<'_>) -> fmt::Result {
    f.write_str(&hex::encode(bytes))
}

fn show_newtype(ty: &str, bytes: &[u8], f: &mut fmt::Formatter<'_>) -> fmt::Result {
    write!(f, "{ty}({})", hex::encode(bytes))
}

fn show_struct(
    ty: Option<&str>,
    bytes: &[u8],
    show_raw: bool,
    f: &mut fmt::Formatter<'_>,
) -> fmt::Result {
    let ty = ty.unwrap_or(DEFAULT_BYTES_TYPE_NAME);
    let mut out = f.debug_struct(ty);
    out.field("len", &bytes.len());
    if show_raw {
        out.field("raw", &hex::encode(bytes));
    }
    out.finish()
}

pub fn pretty_with<F>(f: F) -> impl Debug
where
    F: Fn(&mut fmt::Formatter<'_>) -> fmt::Result,
{
    PrettyWith(f)
}

struct PrettyWith<F>(F);

impl<F> Debug for PrettyWith<F>
where
    F: Fn(&mut fmt::Formatter<'_>) -> fmt::Result,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.0(f)
    }
}

pub fn pretty_group_id(id: &[u8]) -> impl Debug + '_ {
    pretty_bytes(id).show_len(false).show_raw(true)
}

#[cfg(test)]
mod tests {
    use crate::debug::pretty_bytes;

    #[test]
    fn default_format_contains_only_length() {
        let bytes = pretty_bytes(b"foobar");
        let output = format!("{bytes:?}");
        assert!(output.contains("len"));
        assert!(!output.contains("raw"));
        assert!(!output.contains(&hex::encode(b"foobar")));
    }

    #[test]
    fn alternate_format_contains_length_and_hex_encoded_bytes() {
        let bytes = pretty_bytes(b"foobar");
        let output = format!("{bytes:#?}");
        assert!(output.contains("len"));
        assert!(output.contains("raw"));
        assert!(output.contains(&hex::encode(b"foobar")));
    }
}
