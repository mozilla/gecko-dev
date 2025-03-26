use std::convert::Infallible;
use std::fmt::{self, Formatter, Write};
use std::ops::Deref;
use std::pin::Pin;
use std::{borrow, str};

/// Marks a string (or other `Display` type) as safe
///
/// Use this if you want to allow markup in an expression, or if you know
/// that the expression's contents don't need to be escaped.
///
/// Rinja will automatically insert the first (`Escaper`) argument,
/// so this filter only takes a single argument of any type that implements
/// `Display`.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
/// /// ```jinja
/// /// <div>{{ example|safe }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     example: &'a str,
/// }
///
/// assert_eq!(
///     Example { example: "<p>I'm Safe</p>" }.to_string(),
///     "<div><p>I'm Safe</p></div>"
/// );
/// # }
/// ```
#[inline]
pub fn safe<T, E>(text: T, escaper: E) -> Result<Safe<T>, Infallible> {
    let _ = escaper; // it should not be part of the interface that the `escaper` is unused
    Ok(Safe(text))
}

/// Escapes strings according to the escape mode.
///
/// Rinja will automatically insert the first (`Escaper`) argument,
/// so this filter only takes a single argument of any type that implements
/// `Display`.
///
/// It is possible to optionally specify an escaper other than the default for
/// the template's extension, like `{{ val|escape("txt") }}`.
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
/// /// ```jinja
/// /// <div>{{ example|escape }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     example: &'a str,
/// }
///
/// assert_eq!(
///     Example { example: "Escape <>&" }.to_string(),
///     "<div>Escape &#60;&#62;&#38;</div>"
/// );
/// # }
/// ```
#[inline]
pub fn escape<T, E>(text: T, escaper: E) -> Result<Safe<EscapeDisplay<T, E>>, Infallible> {
    Ok(Safe(EscapeDisplay(text, escaper)))
}

pub struct EscapeDisplay<T, E>(T, E);

impl<T: fmt::Display, E: Escaper> fmt::Display for EscapeDisplay<T, E> {
    #[inline]
    fn fmt(&self, fmt: &mut Formatter<'_>) -> fmt::Result {
        write!(EscapeWriter(fmt, self.1), "{}", &self.0)
    }
}

impl<T: FastWritable, E: Escaper> FastWritable for EscapeDisplay<T, E> {
    #[inline]
    fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
        self.0.write_into(&mut EscapeWriter(dest, self.1))
    }
}

struct EscapeWriter<W, E>(W, E);

impl<W: Write, E: Escaper> Write for EscapeWriter<W, E> {
    #[inline]
    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.1.write_escaped_str(&mut self.0, s)
    }

    #[inline]
    fn write_char(&mut self, c: char) -> fmt::Result {
        self.1.write_escaped_char(&mut self.0, c)
    }
}

/// Alias for [`escape()`]
///
/// ```
/// # #[cfg(feature = "code-in-doc")] {
/// # use rinja::Template;
/// /// ```jinja
/// /// <div>{{ example|e }}</div>
/// /// ```
/// #[derive(Template)]
/// #[template(ext = "html", in_doc = true)]
/// struct Example<'a> {
///     example: &'a str,
/// }
///
/// assert_eq!(
///     Example { example: "Escape <>&" }.to_string(),
///     "<div>Escape &#60;&#62;&#38;</div>"
/// );
/// # }
/// ```
#[inline]
pub fn e<T, E>(text: T, escaper: E) -> Result<Safe<EscapeDisplay<T, E>>, Infallible> {
    escape(text, escaper)
}

/// Escape characters in a safe way for HTML texts and attributes
///
/// * `"` => `&#34;`
/// * `&` => `&#38;`
/// * `'` => `&#39;`
/// * `<` => `&#60;`
/// * `>` => `&#62;`
#[derive(Debug, Clone, Copy, Default)]
pub struct Html;

impl Escaper for Html {
    #[inline]
    fn write_escaped_str<W: Write>(&self, fmt: W, string: &str) -> fmt::Result {
        crate::html::write_escaped_str(fmt, string)
    }

    #[inline]
    fn write_escaped_char<W: Write>(&self, fmt: W, c: char) -> fmt::Result {
        crate::html::write_escaped_char(fmt, c)
    }
}

/// Don't escape the input but return in verbatim
#[derive(Debug, Clone, Copy, Default)]
pub struct Text;

impl Escaper for Text {
    #[inline]
    fn write_escaped_str<W: Write>(&self, mut fmt: W, string: &str) -> fmt::Result {
        fmt.write_str(string)
    }

    #[inline]
    fn write_escaped_char<W: Write>(&self, mut fmt: W, c: char) -> fmt::Result {
        fmt.write_char(c)
    }
}

/// Escapers are used to make generated text safe for printing in some context.
///
/// E.g. in an [`Html`] context, any and all generated text can be used in HTML/XML text nodes and
/// attributes, without for for maliciously injected data.
pub trait Escaper: Copy {
    /// Escaped the input string `string` into `fmt`
    fn write_escaped_str<W: Write>(&self, fmt: W, string: &str) -> fmt::Result;

    /// Escaped the input char `c` into `fmt`
    #[inline]
    fn write_escaped_char<W: Write>(&self, fmt: W, c: char) -> fmt::Result {
        self.write_escaped_str(fmt, c.encode_utf8(&mut [0; 4]))
    }
}

/// Used internally by rinja to select the appropriate escaper
pub trait AutoEscape {
    /// The wrapped or converted result type
    type Escaped: fmt::Display;
    /// Early error testing for the input value, usually [`Infallible`]
    type Error: Into<crate::Error>;

    /// Used internally by rinja to select the appropriate escaper
    fn rinja_auto_escape(&self) -> Result<Self::Escaped, Self::Error>;
}

/// Used internally by rinja to select the appropriate escaper
#[derive(Debug, Clone)]
pub struct AutoEscaper<'a, T: ?Sized, E> {
    text: &'a T,
    escaper: E,
}

impl<'a, T: ?Sized, E> AutoEscaper<'a, T, E> {
    /// Used internally by rinja to select the appropriate escaper
    #[inline]
    pub fn new(text: &'a T, escaper: E) -> Self {
        Self { text, escaper }
    }
}

/// Use the provided escaper
impl<'a, T: fmt::Display + ?Sized, E: Escaper> AutoEscape for &&AutoEscaper<'a, T, E> {
    type Escaped = EscapeDisplay<&'a T, E>;
    type Error = Infallible;

    #[inline]
    fn rinja_auto_escape(&self) -> Result<Self::Escaped, Self::Error> {
        Ok(EscapeDisplay(self.text, self.escaper))
    }
}

/// Types that implement this marker trait don't need to be HTML escaped
///
/// Please note that this trait is only meant as speed-up helper. In some odd circumcises rinja
/// might still decide to HTML escape the input, so if this must not happen, then you need to use
/// the [`|safe`](super::safe) filter to prevent the auto escaping.
///
/// If you are unsure if your type generates HTML safe output in all cases, then DON'T mark it.
/// Better safe than sorry!
pub trait HtmlSafe: fmt::Display {}

/// Don't escape HTML safe types
impl<'a, T: HtmlSafe + ?Sized> AutoEscape for &AutoEscaper<'a, T, Html> {
    type Escaped = &'a T;
    type Error = Infallible;

    #[inline]
    fn rinja_auto_escape(&self) -> Result<Self::Escaped, Self::Error> {
        Ok(self.text)
    }
}

/// Mark the output of a filter as "maybe safe"
///
/// This enum can be used as a transparent return type of custom filters that want to mark
/// their output as "safe" depending on some circumstances, i.e. that their output maybe does not
/// need to be escaped.
///
/// If the filter is not used as the last element in the filter chain, then any assumption is void.
/// Let the next filter decide if the output is safe or not.
///
/// ## Example
///
/// ```rust
/// mod filters {
///     use rinja::{filters::MaybeSafe, Result};
///
///     // Do not actually use this filter! It's an intentionally bad example.
///     pub fn backdoor<T: std::fmt::Display>(s: T, enable: &bool) -> Result<MaybeSafe<T>> {
///         Ok(match *enable {
///             true => MaybeSafe::Safe(s),
///             false => MaybeSafe::NeedsEscaping(s),
///         })
///     }
/// }
///
/// #[derive(rinja::Template)]
/// #[template(
///     source = "<div class='{{ klass|backdoor(enable_backdoor) }}'></div>",
///     ext = "html"
/// )]
/// struct DivWithBackdoor<'a> {
///     klass: &'a str,
///     enable_backdoor: bool,
/// }
///
/// assert_eq!(
///     DivWithBackdoor { klass: "<script>", enable_backdoor: false }.to_string(),
///     "<div class='&#60;script&#62;'></div>",
/// );
/// assert_eq!(
///     DivWithBackdoor { klass: "<script>", enable_backdoor: true }.to_string(),
///     "<div class='<script>'></div>",
/// );
/// ```
pub enum MaybeSafe<T> {
    /// The contained value does not need escaping
    Safe(T),
    /// The contained value needs to be escaped
    NeedsEscaping(T),
}

const _: () = {
    // This is the fallback. The filter is not the last element of the filter chain.
    impl<T: fmt::Display> fmt::Display for MaybeSafe<T> {
        #[inline]
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            let inner = match self {
                MaybeSafe::Safe(inner) => inner,
                MaybeSafe::NeedsEscaping(inner) => inner,
            };
            write!(f, "{inner}")
        }
    }

    // This is the fallback. The filter is not the last element of the filter chain.
    impl<T: FastWritable> FastWritable for MaybeSafe<T> {
        #[inline]
        fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
            let inner = match self {
                MaybeSafe::Safe(inner) => inner,
                MaybeSafe::NeedsEscaping(inner) => inner,
            };
            inner.write_into(dest)
        }
    }

    macro_rules! add_ref {
        ($([$($tt:tt)*])*) => { $(
            impl<'a, T: fmt::Display, E: Escaper> AutoEscape
            for &AutoEscaper<'a, $($tt)* MaybeSafe<T>, E> {
                type Escaped = Wrapped<'a, T, E>;
                type Error = Infallible;

                #[inline]
                fn rinja_auto_escape(&self) -> Result<Self::Escaped, Self::Error> {
                    match self.text {
                        MaybeSafe::Safe(t) => Ok(Wrapped::Safe(t)),
                        MaybeSafe::NeedsEscaping(t) => Ok(Wrapped::NeedsEscaping(t, self.escaper)),
                    }
                }
            }
        )* };
    }

    add_ref!([] [&] [&&] [&&&]);

    pub enum Wrapped<'a, T: ?Sized, E> {
        Safe(&'a T),
        NeedsEscaping(&'a T, E),
    }

    impl<T: FastWritable + ?Sized, E: Escaper> FastWritable for Wrapped<'_, T, E> {
        fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
            match *self {
                Wrapped::Safe(t) => t.write_into(dest),
                Wrapped::NeedsEscaping(t, e) => EscapeDisplay(t, e).write_into(dest),
            }
        }
    }

    impl<T: fmt::Display + ?Sized, E: Escaper> fmt::Display for Wrapped<'_, T, E> {
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            match *self {
                Wrapped::Safe(t) => write!(f, "{t}"),
                Wrapped::NeedsEscaping(t, e) => EscapeDisplay(t, e).fmt(f),
            }
        }
    }
};

/// Mark the output of a filter as "safe"
///
/// This struct can be used as a transparent return type of custom filters that want to mark their
/// output as "safe" no matter what, i.e. that their output does not need to be escaped.
///
/// If the filter is not used as the last element in the filter chain, then any assumption is void.
/// Let the next filter decide if the output is safe or not.
///
/// ## Example
///
/// ```rust
/// mod filters {
///     use rinja::{filters::Safe, Result};
///
///     // Do not actually use this filter! It's an intentionally bad example.
///     pub fn strip_except_apos(s: impl ToString) -> Result<Safe<String>> {
///         Ok(Safe(s
///             .to_string()
///             .chars()
///             .filter(|c| !matches!(c, '<' | '>' | '"' | '&'))
///             .collect()
///         ))
///     }
/// }
///
/// #[derive(rinja::Template)]
/// #[template(
///     source = "<div class='{{ klass|strip_except_apos }}'></div>",
///     ext = "html"
/// )]
/// struct DivWithClass<'a> {
///     klass: &'a str,
/// }
///
/// assert_eq!(
///     DivWithClass { klass: "<&'lifetime X>" }.to_string(),
///     "<div class=''lifetime X'></div>",
/// );
/// ```
pub struct Safe<T>(pub T);

const _: () = {
    // This is the fallback. The filter is not the last element of the filter chain.
    impl<T: fmt::Display> fmt::Display for Safe<T> {
        #[inline]
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            write!(f, "{}", self.0)
        }
    }

    // This is the fallback. The filter is not the last element of the filter chain.
    impl<T: FastWritable> FastWritable for Safe<T> {
        #[inline]
        fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
            self.0.write_into(dest)
        }
    }

    macro_rules! add_ref {
        ($([$($tt:tt)*])*) => { $(
            impl<'a, T: fmt::Display, E> AutoEscape for &AutoEscaper<'a, $($tt)* Safe<T>, E> {
                type Escaped = &'a T;
                type Error = Infallible;

                #[inline]
                fn rinja_auto_escape(&self) -> Result<Self::Escaped, Self::Error> {
                    Ok(&self.text.0)
                }
            }
        )* };
    }

    add_ref!([] [&] [&&] [&&&]);
};

/// There is not need to mark the output of a custom filter as "unsafe"; this is simply the default
pub struct Unsafe<T>(pub T);

impl<T: fmt::Display> fmt::Display for Unsafe<T> {
    #[inline]
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// Like [`Safe`], but only for HTML output
pub struct HtmlSafeOutput<T>(pub T);

impl<T: fmt::Display> fmt::Display for HtmlSafeOutput<T> {
    #[inline]
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

macro_rules! mark_html_safe {
    ($($ty:ty),* $(,)?) => {$(
        impl HtmlSafe for $ty {}
    )*};
}

mark_html_safe! {
    bool,
    f32, f64,
    i8, i16, i32, i64, i128, isize,
    u8, u16, u32, u64, u128, usize,
    std::num::NonZeroI8, std::num::NonZeroI16, std::num::NonZeroI32,
    std::num::NonZeroI64, std::num::NonZeroI128, std::num::NonZeroIsize,
    std::num::NonZeroU8, std::num::NonZeroU16, std::num::NonZeroU32,
    std::num::NonZeroU64, std::num::NonZeroU128, std::num::NonZeroUsize,
}

impl<T: HtmlSafe + ?Sized> HtmlSafe for &T {}
impl<T: HtmlSafe + ?Sized> HtmlSafe for Box<T> {}
impl<T: HtmlSafe + ?Sized> HtmlSafe for std::cell::Ref<'_, T> {}
impl<T: HtmlSafe + ?Sized> HtmlSafe for std::cell::RefMut<'_, T> {}
impl<T: HtmlSafe + ?Sized> HtmlSafe for std::rc::Rc<T> {}
impl<T: HtmlSafe + ?Sized> HtmlSafe for std::pin::Pin<&T> {}
impl<T: HtmlSafe + ?Sized> HtmlSafe for std::sync::Arc<T> {}
impl<T: HtmlSafe + ?Sized> HtmlSafe for std::sync::MutexGuard<'_, T> {}
impl<T: HtmlSafe + ?Sized> HtmlSafe for std::sync::RwLockReadGuard<'_, T> {}
impl<T: HtmlSafe + ?Sized> HtmlSafe for std::sync::RwLockWriteGuard<'_, T> {}
impl<T: HtmlSafe> HtmlSafe for std::num::Wrapping<T> {}
impl<T: fmt::Display> HtmlSafe for HtmlSafeOutput<T> {}

impl<T> HtmlSafe for borrow::Cow<'_, T>
where
    T: HtmlSafe + borrow::ToOwned + ?Sized,
    T::Owned: HtmlSafe,
{
}

/// Used internally by rinja to select the appropriate [`write!()`] mechanism
pub struct Writable<'a, S: ?Sized>(pub &'a S);

/// Used internally by rinja to select the appropriate [`write!()`] mechanism
pub trait WriteWritable {
    /// Used internally by rinja to select the appropriate [`write!()`] mechanism
    fn rinja_write<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result;
}

/// Used internally by rinja to speed up writing some types.
///
/// Types implementing this trait can be written without needing to employ an [`fmt::Formatter`].
pub trait FastWritable {
    /// Used internally by rinja to speed up writing some types.
    fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result;
}

const _: () = {
    crate::impl_for_ref! {
        impl FastWritable for T {
            #[inline]
            fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
                <T>::write_into(self, dest)
            }
        }
    }

    impl<T> FastWritable for Pin<T>
    where
        T: Deref,
        <T as Deref>::Target: FastWritable,
    {
        #[inline]
        fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
            self.as_ref().get_ref().write_into(dest)
        }
    }

    impl<T: FastWritable + ToOwned> FastWritable for borrow::Cow<'_, T> {
        #[inline]
        fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
            T::write_into(self.as_ref(), dest)
        }
    }

    // implement FastWritable for a list of types
    macro_rules! impl_for_int {
        ($($ty:ty)*) => { $(
            impl FastWritable for $ty {
                #[inline]
                fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
                    dest.write_str(itoa::Buffer::new().format(*self))
                }
            }
        )* };
    }

    impl_for_int!(
        u8 u16 u32 u64 u128 usize
        i8 i16 i32 i64 i128 isize
    );

    // implement FastWritable for a list of non-zero integral types
    macro_rules! impl_for_nz_int {
        ($($id:ident)*) => { $(
            impl FastWritable for core::num::$id {
                #[inline]
                fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
                    dest.write_str(itoa::Buffer::new().format(self.get()))
                }
            }
        )* };
    }

    impl_for_nz_int!(
        NonZeroU8 NonZeroU16 NonZeroU32 NonZeroU64 NonZeroU128 NonZeroUsize
        NonZeroI8 NonZeroI16 NonZeroI32 NonZeroI64 NonZeroI128 NonZeroIsize
    );

    impl FastWritable for str {
        #[inline]
        fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
            dest.write_str(self)
        }
    }

    impl FastWritable for String {
        #[inline]
        fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
            dest.write_str(self)
        }
    }

    impl FastWritable for bool {
        #[inline]
        fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
            dest.write_str(match self {
                true => "true",
                false => "false",
            })
        }
    }

    impl FastWritable for char {
        #[inline]
        fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
            dest.write_char(*self)
        }
    }

    impl FastWritable for fmt::Arguments<'_> {
        fn write_into<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
            match self.as_str() {
                Some(s) => dest.write_str(s),
                None => dest.write_fmt(*self),
            }
        }
    }

    impl<'a, S: FastWritable + ?Sized> WriteWritable for &Writable<'a, S> {
        #[inline]
        fn rinja_write<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
            self.0.write_into(dest)
        }
    }

    impl<'a, S: fmt::Display + ?Sized> WriteWritable for &&Writable<'a, S> {
        #[inline]
        fn rinja_write<W: fmt::Write + ?Sized>(&self, dest: &mut W) -> fmt::Result {
            write!(dest, "{}", self.0)
        }
    }
};

#[test]
fn test_escape() {
    assert_eq!(escape("", Html).unwrap().to_string(), "");
    assert_eq!(escape("<&>", Html).unwrap().to_string(), "&#60;&#38;&#62;");
    assert_eq!(escape("bla&", Html).unwrap().to_string(), "bla&#38;");
    assert_eq!(escape("<foo", Html).unwrap().to_string(), "&#60;foo");
    assert_eq!(escape("bla&h", Html).unwrap().to_string(), "bla&#38;h");

    assert_eq!(escape("", Text).unwrap().to_string(), "");
    assert_eq!(escape("<&>", Text).unwrap().to_string(), "<&>");
    assert_eq!(escape("bla&", Text).unwrap().to_string(), "bla&");
    assert_eq!(escape("<foo", Text).unwrap().to_string(), "<foo");
    assert_eq!(escape("bla&h", Text).unwrap().to_string(), "bla&h");
}

#[test]
fn test_html_safe_marker() {
    struct Script1;
    struct Script2;

    impl fmt::Display for Script1 {
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            f.write_str("<script>")
        }
    }

    impl fmt::Display for Script2 {
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            f.write_str("<script>")
        }
    }

    impl HtmlSafe for Script2 {}

    assert_eq!(
        (&&AutoEscaper::new(&Script1, Html))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "&#60;script&#62;",
    );
    assert_eq!(
        (&&AutoEscaper::new(&Script2, Html))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "<script>",
    );

    assert_eq!(
        (&&AutoEscaper::new(&Script1, Text))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "<script>",
    );
    assert_eq!(
        (&&AutoEscaper::new(&Script2, Text))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "<script>",
    );

    assert_eq!(
        (&&AutoEscaper::new(&Safe(Script1), Html))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "<script>",
    );
    assert_eq!(
        (&&AutoEscaper::new(&Safe(Script2), Html))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "<script>",
    );

    assert_eq!(
        (&&AutoEscaper::new(&Unsafe(Script1), Html))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "&#60;script&#62;",
    );
    assert_eq!(
        (&&AutoEscaper::new(&Unsafe(Script2), Html))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "&#60;script&#62;",
    );

    assert_eq!(
        (&&AutoEscaper::new(&MaybeSafe::Safe(Script1), Html))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "<script>",
    );
    assert_eq!(
        (&&AutoEscaper::new(&MaybeSafe::Safe(Script2), Html))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "<script>",
    );
    assert_eq!(
        (&&AutoEscaper::new(&MaybeSafe::NeedsEscaping(Script1), Html))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "&#60;script&#62;",
    );
    assert_eq!(
        (&&AutoEscaper::new(&MaybeSafe::NeedsEscaping(Script2), Html))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "&#60;script&#62;",
    );

    assert_eq!(
        (&&AutoEscaper::new(&Safe(std::pin::Pin::new(&Script1)), Html))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "<script>",
    );
    assert_eq!(
        (&&AutoEscaper::new(&Safe(std::pin::Pin::new(&Script2)), Html))
            .rinja_auto_escape()
            .unwrap()
            .to_string(),
        "<script>",
    );
}
