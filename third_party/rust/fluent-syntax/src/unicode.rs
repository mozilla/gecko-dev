//! A set of helper functions for unescaping Fluent unicode escape sequences.
//!
//! # Unicode
//!
//! Fluent supports UTF-8 in all FTL resources, but it also allows
//! unicode sequences to be escaped in [`String
//! Literals`](super::ast::InlineExpression::StringLiteral).
//!
//! Four byte sequences are encoded with `\u` and six byte
//! sequences using `\U`.
//! ## Example
//!
//! ```
//! use fluent_syntax::unicode::unescape_unicode_to_string;
//!
//! assert_eq!(
//!     unescape_unicode_to_string("Foo \\u5bd2 Bar"),
//!     "Foo 寒 Bar"
//! );
//!
//! assert_eq!(
//!     unescape_unicode_to_string("Foo \\U01F68A Bar"),
//!     "Foo 🚊 Bar"
//! );
//! ```
//!
//! # Other unescapes
//!
//! This also allows for a char `"` to be present inside an FTL string literal,
//! and for `\` itself to be escaped.
//!
//! ## Example
//!
//! ```
//! use fluent_syntax::unicode::unescape_unicode_to_string;
//!
//! assert_eq!(
//!     unescape_unicode_to_string("Foo \\\" Bar"),
//!     "Foo \" Bar"
//! );
//! assert_eq!(
//!     unescape_unicode_to_string("Foo \\\\ Bar"),
//!     "Foo \\ Bar"
//! );
//! ```
use std::borrow::Cow;
use std::char;
use std::fmt;

const UNKNOWN_CHAR: char = '�';

fn encode_unicode(s: Option<&str>) -> char {
    s.and_then(|s| u32::from_str_radix(s, 16).ok().and_then(char::from_u32))
        .unwrap_or(UNKNOWN_CHAR)
}

/// Unescapes to a writer without allocating.
///
/// ## Example
///
/// ```
/// use fluent_syntax::unicode::unescape_unicode;
///
/// let mut s = String::new();
/// unescape_unicode(&mut s, "Foo \\U01F60A Bar");
/// assert_eq!(s, "Foo 😊 Bar");
/// ```
pub fn unescape_unicode<W>(w: &mut W, input: &str) -> fmt::Result
where
    W: fmt::Write,
{
    if unescape(w, input)? {
        return Ok(());
    }
    w.write_str(input)
}

fn unescape<W>(w: &mut W, input: &str) -> Result<bool, std::fmt::Error>
where
    W: fmt::Write,
{
    let bytes = input.as_bytes();

    let mut start = 0;
    let mut ptr = 0;

    while let Some(b) = bytes.get(ptr) {
        if b != &b'\\' {
            ptr += 1;
            continue;
        }
        if start != ptr {
            w.write_str(&input[start..ptr])?;
        }

        ptr += 1;

        let new_char = match bytes.get(ptr) {
            Some(b'\\') => '\\',
            Some(b'"') => '"',
            Some(u @ b'u') | Some(u @ b'U') => {
                let seq_start = ptr + 1;
                let len = if u == &b'u' { 4 } else { 6 };
                ptr += len;
                encode_unicode(input.get(seq_start..seq_start + len))
            }
            _ => UNKNOWN_CHAR,
        };
        ptr += 1;
        w.write_char(new_char)?;
        start = ptr;
    }

    if start == 0 {
        return Ok(false);
    }

    if start != ptr {
        w.write_str(&input[start..ptr])?;
    }
    Ok(true)
}

/// Unescapes to a `Cow<str>` optionally allocating.
///
/// ## Example
///
/// ```
/// use fluent_syntax::unicode::unescape_unicode_to_string;
///
/// assert_eq!(
///     unescape_unicode_to_string("Foo \\U01F60A Bar"),
///     "Foo 😊 Bar"
/// );
/// ```
pub fn unescape_unicode_to_string(input: &str) -> Cow<str> {
    let mut result = String::new();
    let owned = unescape(&mut result, input).expect("String write methods don't Err");
    if owned {
        Cow::Owned(result)
    } else {
        Cow::Borrowed(input)
    }
}
