//! Module for built-in filter functions
//!
//! Contains all the built-in filter functions for use in templates.
//! You can define your own filters, as well.
//!
//! ## Note
//!
//! All **result types of any filter function** in this module is **subject to change** at any
//! point, and is **not indicated by as semver breaking** version bump.
//! The traits [`AutoEscape`] and [`WriteWritable`] are used by [`rinja_derive`]'s generated code
//! to work with all compatible types.

mod builtin;
mod escape;
#[cfg(feature = "humansize")]
mod humansize;
#[cfg(feature = "serde_json")]
mod json;
#[cfg(feature = "urlencode")]
mod urlencode;

pub use self::builtin::{
    PluralizeCount, capitalize, center, fmt, format, indent, join, linebreaks, linebreaksbr, lower,
    lowercase, paragraphbreaks, pluralize, title, trim, truncate, upper, uppercase, wordcount,
};
pub use self::escape::{
    AutoEscape, AutoEscaper, Escaper, FastWritable, Html, HtmlSafe, HtmlSafeOutput, MaybeSafe,
    Safe, Text, Unsafe, Writable, WriteWritable, e, escape, safe,
};
#[cfg(feature = "humansize")]
pub use self::humansize::filesizeformat;
#[cfg(feature = "serde_json")]
pub use self::json::{AsIndent, json, json_pretty};
#[cfg(feature = "urlencode")]
pub use self::urlencode::{urlencode, urlencode_strict};
