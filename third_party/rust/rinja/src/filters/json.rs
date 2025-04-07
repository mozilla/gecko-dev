use std::convert::Infallible;
use std::ops::Deref;
use std::pin::Pin;
use std::{fmt, io, str};

use serde::Serialize;
use serde_json::ser::{PrettyFormatter, Serializer, to_writer};

use super::FastWritable;

/// Serialize to JSON (requires `json` feature)
///
/// The generated string does not contain ampersands `&`, chevrons `< >`, or apostrophes `'`.
/// To use it in a `<script>` you can combine it with the safe filter:
///
/// ``` html
/// <script>
/// var data = {{data|json|safe}};
/// </script>
/// ```
///
/// To use it in HTML attributes, you can either use it in quotation marks `"{{data|json}}"` as is,
/// or in apostrophes with the (optional) safe filter `'{{data|json|safe}}'`.
/// In HTML texts the output of e.g. `<pre>{{data|json|safe}}</pre>` is safe, too.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
/// /// ```jinja
/// /// <div><li data-extra='{{data|json|safe}}'>Example</li></div>
/// /// ```
///
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     data: Vec<&'a str>,
/// }
///
/// assert_eq!(
///     Example { data: vec!["foo", "bar"] }.to_string(),
///     "<div><li data-extra='[\"foo\",\"bar\"]'>Example</li></div>"
/// );
/// # }
/// ```
#[inline]
pub fn json(value: impl Serialize) -> Result<impl fmt::Display, Infallible> {
    Ok(ToJson { value })
}

/// Serialize to formatted/prettified JSON (requires `json` feature)
///
/// This filter works the same as [`json()`], but it formats the data for human readability.
/// It has an additional "indent" argument, which can either be an integer how many spaces to use
/// for indentation (capped to 16 characters), or a string (e.g. `"\u{A0}\u{A0}"` for two
/// non-breaking spaces).
///
/// ### Note
///
/// In rinja's template language, this filter is called `|json`, too. The right function is
/// automatically selected depending on whether an `indent` argument was provided or not.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
/// /// ```jinja
/// /// <div>{{data|json(4)|safe}}</div>
/// /// ```
///
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     data: Vec<&'a str>,
/// }
///
/// assert_eq!(
///     Example { data: vec!["foo", "bar"] }.to_string(),
///     "<div>[
///     \"foo\",
///     \"bar\"
/// ]</div>"
/// );
/// # }
/// ```
#[inline]
pub fn json_pretty(
    value: impl Serialize,
    indent: impl AsIndent,
) -> Result<impl fmt::Display, Infallible> {
    Ok(ToJsonPretty { value, indent })
}

#[derive(Debug, Clone)]
struct ToJson<S> {
    value: S,
}

#[derive(Debug, Clone)]
struct ToJsonPretty<S, I> {
    value: S,
    indent: I,
}

/// A prefix usable for indenting [prettified JSON data](json_pretty)
///
/// ```
/// # use rinja::filters::AsIndent;
/// assert_eq!(4.as_indent(), "    ");
/// assert_eq!(" -> ".as_indent(), " -> ");
/// ```
pub trait AsIndent {
    /// Borrow `self` as prefix to use.
    fn as_indent(&self) -> &str;
}

impl AsIndent for str {
    #[inline]
    fn as_indent(&self) -> &str {
        self
    }
}

impl AsIndent for String {
    #[inline]
    fn as_indent(&self) -> &str {
        self
    }
}

impl AsIndent for usize {
    #[inline]
    fn as_indent(&self) -> &str {
        const MAX_SPACES: usize = 16;
        const SPACES: &str = match str::from_utf8(&[b' '; MAX_SPACES]) {
            Ok(spaces) => spaces,
            Err(_) => panic!(),
        };

        &SPACES[..(*self).min(SPACES.len())]
    }
}

impl<T: AsIndent + ToOwned + ?Sized> AsIndent for std::borrow::Cow<'_, T> {
    #[inline]
    fn as_indent(&self) -> &str {
        T::as_indent(self)
    }
}

crate::impl_for_ref! {
    impl AsIndent for T {
        #[inline]
        fn as_indent(&self) -> &str {
            <T>::as_indent(self)
        }
    }
}

impl<T> AsIndent for Pin<T>
where
    T: Deref,
    <T as Deref>::Target: AsIndent,
{
    #[inline]
    fn as_indent(&self) -> &str {
        self.as_ref().get_ref().as_indent()
    }
}

impl<S: Serialize> FastWritable for ToJson<S> {
    #[inline]
    fn write_into<W: fmt::Write + ?Sized>(&self, f: &mut W) -> fmt::Result {
        fmt_json(f, &self.value)
    }
}

impl<S: Serialize> fmt::Display for ToJson<S> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt_json(f, &self.value)
    }
}

impl<S: Serialize, I: AsIndent> FastWritable for ToJsonPretty<S, I> {
    #[inline]
    fn write_into<W: fmt::Write + ?Sized>(&self, f: &mut W) -> fmt::Result {
        fmt_json_pretty(f, &self.value, self.indent.as_indent())
    }
}

impl<S: Serialize, I: AsIndent> fmt::Display for ToJsonPretty<S, I> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt_json_pretty(f, &self.value, self.indent.as_indent())
    }
}

fn fmt_json<S: Serialize, W: fmt::Write + ?Sized>(dest: &mut W, value: &S) -> fmt::Result {
    to_writer(JsonWriter(dest), value).map_err(|_| fmt::Error)
}

fn fmt_json_pretty<S: Serialize, W: fmt::Write + ?Sized>(
    dest: &mut W,
    value: &S,
    indent: &str,
) -> fmt::Result {
    let formatter = PrettyFormatter::with_indent(indent.as_bytes());
    let mut serializer = Serializer::with_formatter(JsonWriter(dest), formatter);
    value.serialize(&mut serializer).map_err(|_| fmt::Error)
}

struct JsonWriter<'a, W: fmt::Write + ?Sized>(&'a mut W);

impl<W: fmt::Write + ?Sized> io::Write for JsonWriter<'_, W> {
    #[inline]
    fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
        self.write_all(bytes)?;
        Ok(bytes.len())
    }

    #[inline]
    fn write_all(&mut self, bytes: &[u8]) -> io::Result<()> {
        write(self.0, bytes).map_err(|err| io::Error::new(io::ErrorKind::InvalidData, err))
    }

    #[inline]
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

fn write<W: fmt::Write + ?Sized>(f: &mut W, bytes: &[u8]) -> fmt::Result {
    let mut last = 0;
    for (index, byte) in bytes.iter().enumerate() {
        let escaped = match byte {
            b'&' => Some(br"\u0026"),
            b'\'' => Some(br"\u0027"),
            b'<' => Some(br"\u003c"),
            b'>' => Some(br"\u003e"),
            _ => None,
        };
        if let Some(escaped) = escaped {
            f.write_str(unsafe { str::from_utf8_unchecked(&bytes[last..index]) })?;
            f.write_str(unsafe { str::from_utf8_unchecked(escaped) })?;
            last = index + 1;
        }
    }
    f.write_str(unsafe { str::from_utf8_unchecked(&bytes[last..]) })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ugly() {
        assert_eq!(json(true).unwrap().to_string(), "true");
        assert_eq!(json("foo").unwrap().to_string(), r#""foo""#);
        assert_eq!(json(true).unwrap().to_string(), "true");
        assert_eq!(json("foo").unwrap().to_string(), r#""foo""#);
        assert_eq!(
            json("<script>").unwrap().to_string(),
            r#""\u003cscript\u003e""#
        );
        assert_eq!(
            json(vec!["foo", "bar"]).unwrap().to_string(),
            r#"["foo","bar"]"#
        );
    }

    #[test]
    fn test_pretty() {
        assert_eq!(json_pretty(true, "").unwrap().to_string(), "true");
        assert_eq!(
            json_pretty("<script>", "").unwrap().to_string(),
            r#""\u003cscript\u003e""#
        );
        assert_eq!(
            json_pretty(vec!["foo", "bar"], "").unwrap().to_string(),
            r#"[
"foo",
"bar"
]"#
        );
        assert_eq!(
            json_pretty(vec!["foo", "bar"], 2).unwrap().to_string(),
            r#"[
  "foo",
  "bar"
]"#
        );
        assert_eq!(
            json_pretty(vec!["foo", "bar"], "————").unwrap().to_string(),
            r#"[
————"foo",
————"bar"
]"#
        );
    }
}
