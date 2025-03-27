use std::cell::Cell;
use std::convert::Infallible;
use std::fmt::{self, Write};
use std::ops::Deref;
use std::pin::Pin;

use super::escape::{FastWritable, HtmlSafeOutput};
use crate::{Error, Result};

// MAX_LEN is maximum allowed length for filters.
const MAX_LEN: usize = 10_000;

/// Formats arguments according to the specified format
///
/// The *second* argument to this filter must be a string literal (as in normal
/// Rust). The two arguments are passed through to the `format!()`
/// [macro](https://doc.rust-lang.org/stable/std/macro.format.html) by
/// the Rinja code generator, but the order is swapped to support filter
/// composition.
///
/// ```ignore
/// {{ value|fmt("{:?}") }}
/// ```
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
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
/// the Rinja code generator.
///
/// ```ignore
/// {{ "{:?}{:?}"|format(value, other_value) }}
/// ```
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
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
/// # use rinja::Template;
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
    fn linebreaks(s: String) -> String {
        let linebroken = s.replace("\n\n", "</p><p>").replace('\n', "<br/>");
        format!("<p>{linebroken}</p>")
    }
    Ok(HtmlSafeOutput(linebreaks(try_to_string(s)?)))
}

/// Converts all newlines in a piece of plain text to HTML line breaks
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
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
    fn linebreaksbr(s: String) -> String {
        s.replace('\n', "<br/>")
    }
    Ok(HtmlSafeOutput(linebreaksbr(try_to_string(s)?)))
}

/// Replaces only paragraph breaks in plain text with appropriate HTML
///
/// A new line followed by a blank line becomes a paragraph break `<p>`.
/// Paragraph tags only wrap content; empty paragraphs are removed.
/// No `<br/>` tags are added.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
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
    fn paragraphbreaks(s: String) -> String {
        let linebroken = s.replace("\n\n", "</p><p>").replace("<p></p>", "");
        format!("<p>{linebroken}</p>")
    }
    Ok(HtmlSafeOutput(paragraphbreaks(try_to_string(s)?)))
}

/// Converts to lowercase
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
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
    fn lower(s: String) -> Result<String, fmt::Error> {
        Ok(s.to_lowercase())
    }
    lower(try_to_string(s)?)
}

/// Converts to lowercase, alias for the `|lower` filter
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
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
/// # use rinja::Template;
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
    fn upper(s: String) -> Result<String, fmt::Error> {
        Ok(s.to_uppercase())
    }
    upper(try_to_string(s)?)
}

/// Converts to uppercase, alias for the `|upper` filter
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
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
/// # use rinja::Template;
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

/// Limit string length, appends '...' if truncated
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
/// /// ```jinja
/// /// <div>{{ example|truncate(2) }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     example: &'a str,
/// }
///
/// assert_eq!(
///     Example { example: "hello" }.to_string(),
///     "<div>he...</div>"
/// );
/// # }
/// ```
#[inline]
pub fn truncate<S: fmt::Display>(
    source: S,
    remaining: usize,
) -> Result<TruncateFilter<S>, Infallible> {
    Ok(TruncateFilter { source, remaining })
}

pub struct TruncateFilter<S> {
    source: S,
    remaining: usize,
}

impl<S: fmt::Display> fmt::Display for TruncateFilter<S> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(TruncateWriter::new(f, self.remaining), "{}", self.source)
    }
}

impl<S: FastWritable> FastWritable for TruncateFilter<S> {
    #[inline]
    fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
        self.source
            .write_into(&mut TruncateWriter::new(dest, self.remaining))
    }
}

struct TruncateWriter<W> {
    dest: Option<W>,
    remaining: usize,
}

impl<W> TruncateWriter<W> {
    fn new(dest: W, remaining: usize) -> Self {
        TruncateWriter {
            dest: Some(dest),
            remaining,
        }
    }
}

impl<W: fmt::Write> fmt::Write for TruncateWriter<W> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let Some(dest) = &mut self.dest else {
            return Ok(());
        };
        let mut rem = self.remaining;
        if rem >= s.len() {
            dest.write_str(s)?;
            self.remaining -= s.len();
        } else {
            if rem > 0 {
                while !s.is_char_boundary(rem) {
                    rem += 1;
                }
                if rem == s.len() {
                    // Don't write "..." if the char bound extends to the end of string.
                    self.remaining = 0;
                    return dest.write_str(s);
                }
                dest.write_str(&s[..rem])?;
            }
            dest.write_str("...")?;
            self.dest = None;
        }
        Ok(())
    }

    #[inline]
    fn write_char(&mut self, c: char) -> fmt::Result {
        match self.dest.is_some() {
            true => self.write_str(c.encode_utf8(&mut [0; 4])),
            false => Ok(()),
        }
    }

    #[inline]
    fn write_fmt(&mut self, args: fmt::Arguments<'_>) -> fmt::Result {
        match self.dest.is_some() {
            true => fmt::write(self, args),
            false => Ok(()),
        }
    }
}

/// Indent lines with `width` spaces
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
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
    fn indent(s: String, width: usize) -> Result<String, fmt::Error> {
        if width >= MAX_LEN || s.len() >= MAX_LEN {
            return Ok(s);
        }
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
    indent(try_to_string(s)?, width)
}

/// Joins iterable into a string separated by provided argument
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
/// /// ```jinja
/// /// <div>{{ example|join(", ") }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     example: &'a [&'a str],
/// }
///
/// assert_eq!(
///     Example { example: &["foo", "bar", "bazz"] }.to_string(),
///     "<div>foo, bar, bazz</div>"
/// );
/// # }
/// ```
#[inline]
pub fn join<I, S>(input: I, separator: S) -> Result<JoinFilter<I, S>, Infallible>
where
    I: IntoIterator,
    I::Item: fmt::Display,
    S: fmt::Display,
{
    Ok(JoinFilter(Cell::new(Some((input, separator)))))
}

/// Result of the filter [`join()`].
///
/// ## Note
///
/// This struct implements [`fmt::Display`], but only produces a string once.
/// Any subsequent call to `.to_string()` will result in an empty string, because the iterator is
/// already consumed.
// The filter contains a [`Cell`], so we can modify iterator inside a method that takes `self` by
// reference: [`fmt::Display::fmt()`] normally has the contract that it will produce the same result
// in multiple invocations for the same object. We break this contract, because have to consume the
// iterator, unless we want to enforce `I: Clone`, nor do we want to "memorize" the result of the
// joined data.
pub struct JoinFilter<I, S>(Cell<Option<(I, S)>>);

impl<I, S> fmt::Display for JoinFilter<I, S>
where
    I: IntoIterator,
    I::Item: fmt::Display,
    S: fmt::Display,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let Some((iter, separator)) = self.0.take() else {
            return Ok(());
        };
        for (idx, token) in iter.into_iter().enumerate() {
            match idx {
                0 => f.write_fmt(format_args!("{token}"))?,
                _ => f.write_fmt(format_args!("{separator}{token}"))?,
            }
        }
        Ok(())
    }
}

/// Capitalize a value. The first character will be uppercase, all others lowercase.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
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
    fn capitalize(s: String) -> Result<String, fmt::Error> {
        match s.chars().next() {
            Some(c) => {
                let mut replacement: String = c.to_uppercase().collect();
                replacement.push_str(&s[c.len_utf8()..].to_lowercase());
                Ok(replacement)
            }
            _ => Ok(s),
        }
    }
    capitalize(try_to_string(s)?)
}

/// Centers the value in a field of a given width
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
/// /// ```jinja
/// /// <div>-{{ example|center(5) }}-</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     example: &'a str,
/// }
///
/// assert_eq!(
///     Example { example: "a" }.to_string(),
///     "<div>-  a  -</div>"
/// );
/// # }
/// ```
#[inline]
pub fn center<T: fmt::Display>(src: T, width: usize) -> Result<Center<T>, Infallible> {
    Ok(Center { src, width })
}

pub struct Center<T> {
    src: T,
    width: usize,
}

impl<T: fmt::Display> fmt::Display for Center<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.width < MAX_LEN {
            write!(f, "{: ^1$}", self.src, self.width)
        } else {
            write!(f, "{}", self.src)
        }
    }
}

/// Count the words in that string.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
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
///     Example { example: "rinja is sort of cool" }.to_string(),
///     "<div>5</div>"
/// );
/// # }
/// ```
#[inline]
pub fn wordcount(s: impl fmt::Display) -> Result<usize, fmt::Error> {
    fn wordcount(s: String) -> Result<usize, fmt::Error> {
        Ok(s.split_whitespace().count())
    }
    wordcount(try_to_string(s)?)
}

/// Return a title cased version of the value. Words will start with uppercase letters, all
/// remaining characters are lowercase.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
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
    let s = try_to_string(s)?;
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

/// For a value of `±1` by default an empty string `""` is returned, otherwise `"s"`.
///
/// # Examples
///
/// ## With default arguments
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
/// /// ```jinja
/// /// I have {{dogs}} dog{{dogs|pluralize}} and {{cats}} cat{{cats|pluralize}}.
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Pets {
///     dogs: i8,
///     cats: i8,
/// }
///
/// assert_eq!(
///     Pets { dogs: 0, cats: 0 }.to_string(),
///     "I have 0 dogs and 0 cats."
/// );
/// assert_eq!(
///     Pets { dogs: 1, cats: 1 }.to_string(),
///     "I have 1 dog and 1 cat."
/// );
/// assert_eq!(
///     Pets { dogs: -1, cats: 99 }.to_string(),
///     "I have -1 dog and 99 cats."
/// );
/// # }
/// ```
///
/// ## Overriding the singular case
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
/// /// ```jinja
/// /// I have {{dogs}} dog{{ dogs|pluralize("go") }}.
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Dog {
///     dogs: i8,
/// }
///
/// assert_eq!(
///     Dog { dogs: 0 }.to_string(),
///     "I have 0 dogs."
/// );
/// assert_eq!(
///     Dog { dogs: 1 }.to_string(),
///     "I have 1 doggo."
/// );
/// # }
/// ```
///
/// ## Overriding singular and plural cases
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
/// /// ```jinja
/// /// I have {{mice}} {{ mice|pluralize("mouse", "mice") }}.
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Mice {
///     mice: i8,
/// }
///
/// assert_eq!(
///     Mice { mice: 42 }.to_string(),
///     "I have 42 mice."
/// );
/// assert_eq!(
///     Mice { mice: 1 }.to_string(),
///     "I have 1 mouse."
/// );
/// # }
/// ```
///
/// ## Arguments get escaped
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
/// /// ```jinja
/// /// You are number {{ number|pluralize("<b>ONE</b>", number) }}!
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Number {
///     number: usize
/// }
///
/// assert_eq!(
///     Number { number: 1 }.to_string(),
///     "You are number &#60;b&#62;ONE&#60;/b&#62;!",
/// );
/// assert_eq!(
///     Number { number: 9000 }.to_string(),
///     "You are number 9000!",
/// );
/// # }
/// ```
#[inline]
pub fn pluralize<C, S, P>(count: C, singular: S, plural: P) -> Result<Pluralize<S, P>, C::Error>
where
    C: PluralizeCount,
{
    match count.is_singular()? {
        true => Ok(Pluralize::Singular(singular)),
        false => Ok(Pluralize::Plural(plural)),
    }
}

/// An integer that can have the value `+1` and maybe `-1`.
pub trait PluralizeCount {
    /// A possible error that can occur while checking the value.
    type Error: Into<Error>;

    /// Returns `true` if and only if the value is `±1`.
    fn is_singular(&self) -> Result<bool, Self::Error>;
}

const _: () = {
    crate::impl_for_ref! {
        impl PluralizeCount for T {
            type Error = T::Error;

            #[inline]
            fn is_singular(&self) -> Result<bool, Self::Error> {
                <T>::is_singular(self)
            }
        }
    }

    impl<T> PluralizeCount for Pin<T>
    where
        T: Deref,
        <T as Deref>::Target: PluralizeCount,
    {
        type Error = <<T as Deref>::Target as PluralizeCount>::Error;

        #[inline]
        fn is_singular(&self) -> Result<bool, Self::Error> {
            self.as_ref().get_ref().is_singular()
        }
    }

    /// implement `PluralizeCount` for unsigned integer types
    macro_rules! impl_pluralize_for_unsigned_int {
        ($($ty:ty)*) => { $(
            impl PluralizeCount for $ty {
                type Error = Infallible;

                #[inline]
                fn is_singular(&self) -> Result<bool, Self::Error> {
                    Ok(*self == 1)
                }
            }
        )* };
    }

    impl_pluralize_for_unsigned_int!(u8 u16 u32 u64 u128 usize);

    /// implement `PluralizeCount` for signed integer types
    macro_rules! impl_pluralize_for_signed_int {
        ($($ty:ty)*) => { $(
            impl PluralizeCount for $ty {
                type Error = Infallible;

                #[inline]
                fn is_singular(&self) -> Result<bool, Self::Error> {
                    Ok(*self == 1 || *self == -1)
                }
            }
        )* };
    }

    impl_pluralize_for_signed_int!(i8 i16 i32 i64 i128 isize);

    /// implement `PluralizeCount` for non-zero integer types
    macro_rules! impl_pluralize_for_non_zero {
        ($($ty:ident)*) => { $(
            impl PluralizeCount for std::num::$ty {
                type Error = Infallible;

                #[inline]
                fn is_singular(&self) -> Result<bool, Self::Error> {
                    self.get().is_singular()
                }
            }
        )* };
    }

    impl_pluralize_for_non_zero! {
        NonZeroI8 NonZeroI16 NonZeroI32 NonZeroI64 NonZeroI128 NonZeroIsize
        NonZeroU8 NonZeroU16 NonZeroU32 NonZeroU64 NonZeroU128 NonZeroUsize
    }
};

pub enum Pluralize<S, P> {
    Singular(S),
    Plural(P),
}

impl<S: fmt::Display, P: fmt::Display> fmt::Display for Pluralize<S, P> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Pluralize::Singular(value) => write!(f, "{value}"),
            Pluralize::Plural(value) => write!(f, "{value}"),
        }
    }
}

impl<S: FastWritable, P: FastWritable> FastWritable for Pluralize<S, P> {
    #[inline]
    fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
        match self {
            Pluralize::Singular(value) => value.write_into(dest),
            Pluralize::Plural(value) => value.write_into(dest),
        }
    }
}

fn try_to_string(s: impl fmt::Display) -> Result<String, fmt::Error> {
    let mut result = String::new();
    write!(result, "{s}")?;
    Ok(result)
}

#[cfg(test)]
mod tests {
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
    fn test_truncate() {
        assert_eq!(truncate("hello", 2).unwrap().to_string(), "he...");
        let a = String::from("您好");
        assert_eq!(a.len(), 6);
        assert_eq!(String::from("您").len(), 3);
        assert_eq!(truncate("您好", 1).unwrap().to_string(), "您...");
        assert_eq!(truncate("您好", 2).unwrap().to_string(), "您...");
        assert_eq!(truncate("您好", 3).unwrap().to_string(), "您...");
        assert_eq!(truncate("您好", 4).unwrap().to_string(), "您好");
        assert_eq!(truncate("您好", 5).unwrap().to_string(), "您好");
        assert_eq!(truncate("您好", 6).unwrap().to_string(), "您好");
        assert_eq!(truncate("您好", 7).unwrap().to_string(), "您好");
        let s = String::from("🤚a🤚");
        assert_eq!(s.len(), 9);
        assert_eq!(String::from("🤚").len(), 4);
        assert_eq!(truncate("🤚a🤚", 1).unwrap().to_string(), "🤚...");
        assert_eq!(truncate("🤚a🤚", 2).unwrap().to_string(), "🤚...");
        assert_eq!(truncate("🤚a🤚", 3).unwrap().to_string(), "🤚...");
        assert_eq!(truncate("🤚a🤚", 4).unwrap().to_string(), "🤚...");
        assert_eq!(truncate("🤚a🤚", 5).unwrap().to_string(), "🤚a...");
        assert_eq!(truncate("🤚a🤚", 6).unwrap().to_string(), "🤚a🤚");
        assert_eq!(truncate("🤚a🤚", 6).unwrap().to_string(), "🤚a🤚");
        assert_eq!(truncate("🤚a🤚", 7).unwrap().to_string(), "🤚a🤚");
        assert_eq!(truncate("🤚a🤚", 8).unwrap().to_string(), "🤚a🤚");
        assert_eq!(truncate("🤚a🤚", 9).unwrap().to_string(), "🤚a🤚");
        assert_eq!(truncate("🤚a🤚", 10).unwrap().to_string(), "🤚a🤚");
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

    #[allow(clippy::needless_borrow)]
    #[test]
    fn test_join() {
        assert_eq!(
            join((&["hello", "world"]).iter(), ", ")
                .unwrap()
                .to_string(),
            "hello, world"
        );
        assert_eq!(
            join((&["hello"]).iter(), ", ").unwrap().to_string(),
            "hello"
        );

        let empty: &[&str] = &[];
        assert_eq!(join(empty.iter(), ", ").unwrap().to_string(), "");

        let input: Vec<String> = vec!["foo".into(), "bar".into(), "bazz".into()];
        assert_eq!(join(input.iter(), ":").unwrap().to_string(), "foo:bar:bazz");

        let input: &[String] = &["foo".into(), "bar".into()];
        assert_eq!(join(input.iter(), ":").unwrap().to_string(), "foo:bar");

        let real: String = "blah".into();
        let input: Vec<&str> = vec![&real];
        assert_eq!(join(input.iter(), ";").unwrap().to_string(), "blah");

        assert_eq!(
            join((&&&&&["foo", "bar"]).iter(), ", ")
                .unwrap()
                .to_string(),
            "foo, bar"
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
    fn test_center() {
        assert_eq!(center("f", 3).unwrap().to_string(), " f ".to_string());
        assert_eq!(center("f", 4).unwrap().to_string(), " f  ".to_string());
        assert_eq!(center("foo", 1).unwrap().to_string(), "foo".to_string());
        assert_eq!(
            center("foo bar", 8).unwrap().to_string(),
            "foo bar ".to_string()
        );
        assert_eq!(
            center("foo", 111_669_149_696).unwrap().to_string(),
            "foo".to_string()
        );
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
