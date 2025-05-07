use alloc::string::String;
use core::fmt::{self, Write};

use super::MAX_LEN;
use super::escape::HtmlSafeOutput;
use crate::Result;

/// Return an ephemeral `&str` for `$src: impl fmt::Display`
///
/// If `$str` is `&str` or `String`, this macro simply passes on its content.
/// If it is neither, then the formatted data is collection into `&buffer`.
///
/// `return`s with an error if the formatting failed.
macro_rules! try_to_str {
    ($src:expr => $buffer:ident) => {
        match format_args!("{}", $src) {
            args => {
                if let Some(s) = args.as_str() {
                    s
                } else {
                    $buffer = String::new();
                    $buffer.write_fmt(args)?;
                    &$buffer
                }
            }
        }
    };
}

/// Formats arguments according to the specified format
///
/// The *second* argument to this filter must be a string literal (as in normal
/// Rust). The two arguments are passed through to the `format!()`
/// [macro](https://doc.rust-lang.org/stable/std/macro.format.html) by
/// the Askama code generator, but the order is swapped to support filter
/// composition.
///
/// ```ignore
/// {{ value|fmt("{:?}") }}
/// ```
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ value|fmt("{:?}") }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example {
///     value: (usize, usize),
/// }
///
/// assert_eq!(
///     Example { value: (3, 4) }.to_string(),
///     "<div>(3, 4)</div>"
/// );
/// # }
/// ```
///
/// Compare with [format](./fn.format.html).
pub fn fmt() {}

/// Formats arguments according to the specified format
///
/// The first argument to this filter must be a string literal (as in normal
/// Rust). All arguments are passed through to the `format!()`
/// [macro](https://doc.rust-lang.org/stable/std/macro.format.html) by
/// the Askama code generator.
///
/// ```ignore
/// {{ "{:?}{:?}"|format(value, other_value) }}
/// ```
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ "{:?}"|format(value) }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example {
///     value: (usize, usize),
/// }
///
/// assert_eq!(
///     Example { value: (3, 4) }.to_string(),
///     "<div>(3, 4)</div>"
/// );
/// # }
/// ```
///
/// Compare with [fmt](./fn.fmt.html).
pub fn format() {}

/// Replaces line breaks in plain text with appropriate HTML
///
/// A single newline becomes an HTML line break `<br>` and a new line
/// followed by a blank line becomes a paragraph break `<p>`.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ example|linebreaks }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     example: &'a str,
/// }
///
/// assert_eq!(
///     Example { example: "Foo\nBar\n\nBaz" }.to_string(),
///     "<div><p>Foo<br/>Bar</p><p>Baz</p></div>"
/// );
/// # }
/// ```
#[inline]
pub fn linebreaks(s: impl fmt::Display) -> Result<HtmlSafeOutput<String>, fmt::Error> {
    fn linebreaks(s: &str) -> String {
        let linebroken = s.replace("\n\n", "</p><p>").replace('\n', "<br/>");
        alloc::format!("<p>{linebroken}</p>")
    }

    let mut buffer;
    Ok(HtmlSafeOutput(linebreaks(try_to_str!(s => buffer))))
}

/// Converts all newlines in a piece of plain text to HTML line breaks
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ lines|linebreaksbr }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     lines: &'a str,
/// }
///
/// assert_eq!(
///     Example { lines: "a\nb\nc" }.to_string(),
///     "<div>a<br/>b<br/>c</div>"
/// );
/// # }
/// ```
#[inline]
pub fn linebreaksbr(s: impl fmt::Display) -> Result<HtmlSafeOutput<String>, fmt::Error> {
    fn linebreaksbr(s: &str) -> String {
        s.replace('\n', "<br/>")
    }

    let mut buffer;
    Ok(HtmlSafeOutput(linebreaksbr(try_to_str!(s => buffer))))
}

/// Replaces only paragraph breaks in plain text with appropriate HTML
///
/// A new line followed by a blank line becomes a paragraph break `<p>`.
/// Paragraph tags only wrap content; empty paragraphs are removed.
/// No `<br/>` tags are added.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// {{ lines|paragraphbreaks }}
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     lines: &'a str,
/// }
///
/// assert_eq!(
///     Example { lines: "Foo\nBar\n\nBaz" }.to_string(),
///     "<p>Foo\nBar</p><p>Baz</p>"
/// );
/// # }
/// ```
#[inline]
pub fn paragraphbreaks(s: impl fmt::Display) -> Result<HtmlSafeOutput<String>, fmt::Error> {
    fn paragraphbreaks(s: &str) -> String {
        let linebroken = s.replace("\n\n", "</p><p>").replace("<p></p>", "");
        alloc::format!("<p>{linebroken}</p>")
    }

    let mut buffer;
    Ok(HtmlSafeOutput(paragraphbreaks(try_to_str!(s => buffer))))
}

/// Converts to lowercase
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ word|lower }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     word: &'a str,
/// }
///
/// assert_eq!(
///     Example { word: "FOO" }.to_string(),
///     "<div>foo</div>"
/// );
///
/// assert_eq!(
///     Example { word: "FooBar" }.to_string(),
///     "<div>foobar</div>"
/// );
/// # }
/// ```
#[inline]
pub fn lower(s: impl fmt::Display) -> Result<String, fmt::Error> {
    let mut buffer;
    Ok(try_to_str!(s => buffer).to_lowercase())
}

/// Converts to lowercase, alias for the `|lower` filter
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ word|lowercase }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     word: &'a str,
/// }
///
/// assert_eq!(
///     Example { word: "FOO" }.to_string(),
///     "<div>foo</div>"
/// );
///
/// assert_eq!(
///     Example { word: "FooBar" }.to_string(),
///     "<div>foobar</div>"
/// );
/// # }
/// ```
#[inline]
pub fn lowercase(s: impl fmt::Display) -> Result<String, fmt::Error> {
    lower(s)
}

/// Converts to uppercase
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ word|upper }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     word: &'a str,
/// }
///
/// assert_eq!(
///     Example { word: "foo" }.to_string(),
///     "<div>FOO</div>"
/// );
///
/// assert_eq!(
///     Example { word: "FooBar" }.to_string(),
///     "<div>FOOBAR</div>"
/// );
/// # }
/// ```
#[inline]
pub fn upper(s: impl fmt::Display) -> Result<String, fmt::Error> {
    let mut buffer;
    Ok(try_to_str!(s => buffer).to_uppercase())
}

/// Converts to uppercase, alias for the `|upper` filter
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ word|uppercase }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     word: &'a str,
/// }
///
/// assert_eq!(
///     Example { word: "foo" }.to_string(),
///     "<div>FOO</div>"
/// );
///
/// assert_eq!(
///     Example { word: "FooBar" }.to_string(),
///     "<div>FOOBAR</div>"
/// );
/// # }
/// ```
#[inline]
pub fn uppercase(s: impl fmt::Display) -> Result<String, fmt::Error> {
    upper(s)
}

/// Strip leading and trailing whitespace
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ example|trim }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     example: &'a str,
/// }
///
/// assert_eq!(
///     Example { example: " Hello\tworld\t" }.to_string(),
///     "<div>Hello\tworld</div>"
/// );
/// # }
/// ```
#[cfg(feature = "alloc")]
pub fn trim<T: fmt::Display>(s: T) -> Result<String> {
    struct Collector(String);

    impl fmt::Write for Collector {
        fn write_str(&mut self, s: &str) -> fmt::Result {
            match self.0.is_empty() {
                true => self.0.write_str(s.trim_start()),
                false => self.0.write_str(s),
            }
        }
    }

    let mut collector = Collector(String::new());
    write!(collector, "{s}")?;
    let Collector(mut s) = collector;
    s.truncate(s.trim_end().len());
    Ok(s)
}

/// Indent lines with `width` spaces
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ example|indent(4) }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     example: &'a str,
/// }
///
/// assert_eq!(
///     Example { example: "hello\nfoo\nbar" }.to_string(),
///     "<div>hello\n    foo\n    bar</div>"
/// );
/// # }
/// ```
#[inline]
pub fn indent(s: impl fmt::Display, width: usize) -> Result<String, fmt::Error> {
    fn indent(args: fmt::Arguments<'_>, width: usize) -> Result<String, fmt::Error> {
        let mut buffer = String::new();
        let s = if width >= MAX_LEN {
            buffer.write_fmt(args)?;
            return Ok(buffer);
        } else if let Some(s) = args.as_str() {
            if s.len() >= MAX_LEN {
                return Ok(s.into());
            } else {
                s
            }
        } else {
            buffer.write_fmt(args)?;
            if buffer.len() >= MAX_LEN {
                return Ok(buffer);
            }
            buffer.as_str()
        };

        let mut indented = String::new();
        for (i, c) in s.char_indices() {
            indented.push(c);

            if c == '\n' && i < s.len() - 1 {
                for _ in 0..width {
                    indented.push(' ');
                }
            }
        }
        Ok(indented)
    }
    indent(format_args!("{s}"), width)
}

/// Capitalize a value. The first character will be uppercase, all others lowercase.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ example|capitalize }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     example: &'a str,
/// }
///
/// assert_eq!(
///     Example { example: "hello" }.to_string(),
///     "<div>Hello</div>"
/// );
///
/// assert_eq!(
///     Example { example: "hElLO" }.to_string(),
///     "<div>Hello</div>"
/// );
/// # }
/// ```
#[inline]
pub fn capitalize(s: impl fmt::Display) -> Result<String, fmt::Error> {
    fn capitalize(s: &str) -> Result<String, fmt::Error> {
        let mut chars = s.chars();
        if let Some(c) = chars.next() {
            let mut replacement = String::with_capacity(s.len());
            replacement.extend(c.to_uppercase());
            replacement.push_str(&chars.as_str().to_lowercase());
            Ok(replacement)
        } else {
            Ok(String::new())
        }
    }

    let mut buffer;
    capitalize(try_to_str!(s => buffer))
}

/// Count the words in that string.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ example|wordcount }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     example: &'a str,
/// }
///
/// assert_eq!(
///     Example { example: "askama is sort of cool" }.to_string(),
///     "<div>5</div>"
/// );
/// # }
/// ```
pub fn wordcount(s: impl fmt::Display) -> Result<usize, fmt::Error> {
    let mut buffer;
    Ok(try_to_str!(s => buffer).split_whitespace().count())
}

/// Return a title cased version of the value. Words will start with uppercase letters, all
/// remaining characters are lowercase.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use askama::Template;
/// /// ```jinja
/// /// <div>{{ example|title }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     example: &'a str,
/// }
///
/// assert_eq!(
///     Example { example: "hello WORLD" }.to_string(),
///     "<div>Hello World</div>"
/// );
/// # }
/// ```
pub fn title(s: impl fmt::Display) -> Result<String, fmt::Error> {
    let mut buffer;
    let s = try_to_str!(s => buffer);
    let mut need_capitalization = true;

    // Sadly enough, we can't mutate a string when iterating over its chars, likely because it could
    // change the size of a char, "breaking" the char indices.
    let mut output = String::with_capacity(s.len());
    for c in s.chars() {
        if c.is_whitespace() {
            output.push(c);
            need_capitalization = true;
        } else if need_capitalization {
            match c.is_uppercase() {
                true => output.push(c),
                false => output.extend(c.to_uppercase()),
            }
            need_capitalization = false;
        } else {
            match c.is_lowercase() {
                true => output.push(c),
                false => output.extend(c.to_lowercase()),
            }
        }
    }
    Ok(output)
}

#[cfg(test)]
mod tests {
    use alloc::string::ToString;

    use super::*;

    #[test]
    fn test_linebreaks() {
        assert_eq!(
            linebreaks("Foo\nBar Baz").unwrap().to_string(),
            "<p>Foo<br/>Bar Baz</p>"
        );
        assert_eq!(
            linebreaks("Foo\nBar\n\nBaz").unwrap().to_string(),
            "<p>Foo<br/>Bar</p><p>Baz</p>"
        );
    }

    #[test]
    fn test_linebreaksbr() {
        assert_eq!(linebreaksbr("Foo\nBar").unwrap().to_string(), "Foo<br/>Bar");
        assert_eq!(
            linebreaksbr("Foo\nBar\n\nBaz").unwrap().to_string(),
            "Foo<br/>Bar<br/><br/>Baz"
        );
    }

    #[test]
    fn test_paragraphbreaks() {
        assert_eq!(
            paragraphbreaks("Foo\nBar Baz").unwrap().to_string(),
            "<p>Foo\nBar Baz</p>"
        );
        assert_eq!(
            paragraphbreaks("Foo\nBar\n\nBaz").unwrap().to_string(),
            "<p>Foo\nBar</p><p>Baz</p>"
        );
        assert_eq!(
            paragraphbreaks("Foo\n\n\n\n\nBar\n\nBaz")
                .unwrap()
                .to_string(),
            "<p>Foo</p><p>\nBar</p><p>Baz</p>"
        );
    }

    #[test]
    fn test_lower() {
        assert_eq!(lower("Foo").unwrap().to_string(), "foo");
        assert_eq!(lower("FOO").unwrap().to_string(), "foo");
        assert_eq!(lower("FooBar").unwrap().to_string(), "foobar");
        assert_eq!(lower("foo").unwrap().to_string(), "foo");
    }

    #[test]
    fn test_upper() {
        assert_eq!(upper("Foo").unwrap().to_string(), "FOO");
        assert_eq!(upper("FOO").unwrap().to_string(), "FOO");
        assert_eq!(upper("FooBar").unwrap().to_string(), "FOOBAR");
        assert_eq!(upper("foo").unwrap().to_string(), "FOO");
    }

    #[test]
    fn test_trim() {
        assert_eq!(trim(" Hello\tworld\t").unwrap().to_string(), "Hello\tworld");
    }

    #[test]
    fn test_indent() {
        assert_eq!(indent("hello", 2).unwrap().to_string(), "hello");
        assert_eq!(indent("hello\n", 2).unwrap().to_string(), "hello\n");
        assert_eq!(indent("hello\nfoo", 2).unwrap().to_string(), "hello\n  foo");
        assert_eq!(
            indent("hello\nfoo\n bar", 4).unwrap().to_string(),
            "hello\n    foo\n     bar"
        );
        assert_eq!(
            indent("hello", 267_332_238_858).unwrap().to_string(),
            "hello"
        );
    }

    #[test]
    fn test_capitalize() {
        assert_eq!(capitalize("foo").unwrap().to_string(), "Foo".to_string());
        assert_eq!(capitalize("f").unwrap().to_string(), "F".to_string());
        assert_eq!(capitalize("fO").unwrap().to_string(), "Fo".to_string());
        assert_eq!(capitalize("").unwrap().to_string(), String::new());
        assert_eq!(capitalize("FoO").unwrap().to_string(), "Foo".to_string());
        assert_eq!(
            capitalize("foO BAR").unwrap().to_string(),
            "Foo bar".to_string()
        );
        assert_eq!(
            capitalize("äØÄÅÖ").unwrap().to_string(),
            "Äøäåö".to_string()
        );
        assert_eq!(capitalize("ß").unwrap().to_string(), "SS".to_string());
        assert_eq!(capitalize("ßß").unwrap().to_string(), "SSß".to_string());
    }

    #[test]
    fn test_wordcount() {
        assert_eq!(wordcount("").unwrap(), 0);
        assert_eq!(wordcount(" \n\t").unwrap(), 0);
        assert_eq!(wordcount("foo").unwrap(), 1);
        assert_eq!(wordcount("foo bar").unwrap(), 2);
        assert_eq!(wordcount("foo  bar").unwrap(), 2);
    }

    #[test]
    fn test_title() {
        assert_eq!(&title("").unwrap(), "");
        assert_eq!(&title(" \n\t").unwrap(), " \n\t");
        assert_eq!(&title("foo").unwrap(), "Foo");
        assert_eq!(&title(" foo").unwrap(), " Foo");
        assert_eq!(&title("foo bar").unwrap(), "Foo Bar");
        assert_eq!(&title("foo  bar ").unwrap(), "Foo  Bar ");
        assert_eq!(&title("fOO").unwrap(), "Foo");
        assert_eq!(&title("fOo BaR").unwrap(), "Foo Bar");
    }

    #[test]
    fn fuzzed_indent_filter() {
        let s = "hello\nfoo\nbar".to_string().repeat(1024);
        assert_eq!(indent(s.clone(), 4).unwrap().to_string(), s);
    }
}
