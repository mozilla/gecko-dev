#![cfg_attr(docsrs, feature(doc_cfg, doc_auto_cfg))]
#![deny(elided_lifetimes_in_paths)]
#![deny(unreachable_pub)]

use std::borrow::Cow;
use std::cell::Cell;
use std::env::current_dir;
use std::ops::{Deref, DerefMut};
use std::path::Path;
use std::sync::Arc;
use std::{fmt, str};

use nom::branch::alt;
use nom::bytes::complete::{escaped, is_not, tag, take_till, take_while_m_n};
use nom::character::complete::{anychar, char, one_of, satisfy};
use nom::combinator::{consumed, cut, fail, map, not, opt, recognize, value};
use nom::error::{ErrorKind, FromExternalError};
use nom::multi::{many0_count, many1};
use nom::sequence::{delimited, pair, preceded, tuple};
use nom::{AsChar, InputTakeAtPosition};

pub mod expr;
pub use expr::{Expr, Filter};
mod memchr_splitter;
pub mod node;
pub use node::Node;

mod target;
pub use target::Target;
#[cfg(test)]
mod tests;

mod _parsed {
    use std::path::Path;
    use std::sync::Arc;
    use std::{fmt, mem};

    use super::node::Node;
    use super::{Ast, ParseError, Syntax};

    pub struct Parsed {
        // `source` must outlive `ast`, so `ast` must be declared before `source`
        ast: Ast<'static>,
        #[allow(dead_code)]
        source: Arc<str>,
    }

    impl Parsed {
        /// If `file_path` is `None`, it means the `source` is an inline template. Therefore, if
        /// a parsing error occurs, we won't display the path as it wouldn't be useful.
        pub fn new(
            source: Arc<str>,
            file_path: Option<Arc<Path>>,
            syntax: &Syntax<'_>,
        ) -> Result<Self, ParseError> {
            // Self-referential borrowing: `self` will keep the source alive as `String`,
            // internally we will transmute it to `&'static str` to satisfy the compiler.
            // However, we only expose the nodes with a lifetime limited to `self`.
            let src = unsafe { mem::transmute::<&str, &'static str>(source.as_ref()) };
            let ast = Ast::from_str(src, file_path, syntax)?;
            Ok(Self { ast, source })
        }

        // The return value's lifetime must be limited to `self` to uphold the unsafe invariant.
        #[must_use]
        pub fn nodes(&self) -> &[Node<'_>] {
            &self.ast.nodes
        }

        #[must_use]
        pub fn source(&self) -> &str {
            &self.source
        }
    }

    impl fmt::Debug for Parsed {
        fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            f.debug_struct("Parsed")
                .field("nodes", &self.ast.nodes)
                .finish_non_exhaustive()
        }
    }

    impl PartialEq for Parsed {
        fn eq(&self, other: &Self) -> bool {
            self.ast.nodes == other.ast.nodes
        }
    }

    impl Default for Parsed {
        fn default() -> Self {
            Self {
                ast: Ast::default(),
                source: "".into(),
            }
        }
    }
}

pub use _parsed::Parsed;

#[derive(Debug, Default)]
pub struct Ast<'a> {
    nodes: Vec<Node<'a>>,
}

impl<'a> Ast<'a> {
    /// If `file_path` is `None`, it means the `source` is an inline template. Therefore, if
    /// a parsing error occurs, we won't display the path as it wouldn't be useful.
    pub fn from_str(
        src: &'a str,
        file_path: Option<Arc<Path>>,
        syntax: &Syntax<'_>,
    ) -> Result<Self, ParseError> {
        match Node::parse_template(src, &State::new(syntax)) {
            Ok(("", nodes)) => Ok(Self { nodes }),
            Ok(_) | Err(nom::Err::Incomplete(_)) => unreachable!(),
            Err(
                nom::Err::Error(ErrorContext { input, message, .. })
                | nom::Err::Failure(ErrorContext { input, message, .. }),
            ) => Err(ParseError {
                message,
                offset: src.len() - input.len(),
                file_path,
            }),
        }
    }

    #[must_use]
    pub fn nodes(&self) -> &[Node<'a>] {
        &self.nodes
    }
}

/// Struct used to wrap types with their associated "span" which is used when generating errors
/// in the code generation.
pub struct WithSpan<'a, T> {
    inner: T,
    span: &'a str,
}

impl<'a, T> WithSpan<'a, T> {
    pub const fn new(inner: T, span: &'a str) -> Self {
        Self { inner, span }
    }

    pub fn span(&self) -> &'a str {
        self.span
    }

    pub fn deconstruct(self) -> (T, &'a str) {
        let Self { inner, span } = self;
        (inner, span)
    }
}

impl<'a, T> Deref for WithSpan<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.inner
    }
}

impl<'a, T> DerefMut for WithSpan<'a, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.inner
    }
}

impl<'a, T: fmt::Debug> fmt::Debug for WithSpan<'a, T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:?}", self.inner)
    }
}

impl<'a, T: Clone> Clone for WithSpan<'a, T> {
    fn clone(&self) -> Self {
        Self {
            inner: self.inner.clone(),
            span: self.span,
        }
    }
}

impl<'a, T: PartialEq> PartialEq for WithSpan<'a, T> {
    fn eq(&self, other: &Self) -> bool {
        // We never want to compare the span information.
        self.inner == other.inner
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParseError {
    pub message: Option<Cow<'static, str>>,
    pub offset: usize,
    pub file_path: Option<Arc<Path>>,
}

impl std::error::Error for ParseError {}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let ParseError {
            message,
            file_path,
            offset,
        } = self;

        if let Some(message) = message {
            writeln!(f, "{message}")?;
        }

        let path = file_path
            .as_ref()
            .and_then(|path| Some(strip_common(&current_dir().ok()?, path)));
        match path {
            Some(path) => write!(f, "failed to parse template source\n  --> {path}@{offset}"),
            None => write!(f, "failed to parse template source near offset {offset}"),
        }
    }
}

pub(crate) type ParseErr<'a> = nom::Err<ErrorContext<'a>>;
pub(crate) type ParseResult<'a, T = &'a str> = Result<(&'a str, T), ParseErr<'a>>;

/// This type is used to handle `nom` errors and in particular to add custom error messages.
/// It used to generate `ParserError`.
///
/// It cannot be used to replace `ParseError` because it expects a generic, which would make
/// `rinja`'s users experience less good (since this generic is only needed for `nom`).
#[derive(Debug)]
pub(crate) struct ErrorContext<'a> {
    pub(crate) input: &'a str,
    pub(crate) message: Option<Cow<'static, str>>,
}

impl<'a> ErrorContext<'a> {
    fn unclosed(kind: &str, tag: &str, i: &'a str) -> Self {
        Self::new(format!("unclosed {kind}, missing {tag:?}"), i)
    }

    fn new(message: impl Into<Cow<'static, str>>, input: &'a str) -> Self {
        Self {
            input,
            message: Some(message.into()),
        }
    }
}

impl<'a> nom::error::ParseError<&'a str> for ErrorContext<'a> {
    fn from_error_kind(input: &'a str, _code: ErrorKind) -> Self {
        Self {
            input,
            message: None,
        }
    }

    fn append(_: &'a str, _: ErrorKind, other: Self) -> Self {
        other
    }
}

impl<'a, E: std::fmt::Display> FromExternalError<&'a str, E> for ErrorContext<'a> {
    fn from_external_error(input: &'a str, _kind: ErrorKind, e: E) -> Self {
        Self {
            input,
            message: Some(Cow::Owned(e.to_string())),
        }
    }
}

impl<'a> From<ErrorContext<'a>> for nom::Err<ErrorContext<'a>> {
    fn from(cx: ErrorContext<'a>) -> Self {
        Self::Failure(cx)
    }
}

fn is_ws(c: char) -> bool {
    matches!(c, ' ' | '\t' | '\r' | '\n')
}

fn not_ws(c: char) -> bool {
    !is_ws(c)
}

fn ws<'a, O>(
    inner: impl FnMut(&'a str) -> ParseResult<'a, O>,
) -> impl FnMut(&'a str) -> ParseResult<'a, O> {
    delimited(take_till(not_ws), inner, take_till(not_ws))
}

/// Skips input until `end` was found, but does not consume it.
/// Returns tuple that would be returned when parsing `end`.
fn skip_till<'a, 'b, O>(
    candidate_finder: impl crate::memchr_splitter::Splitter,
    end: impl FnMut(&'a str) -> ParseResult<'a, O>,
) -> impl FnMut(&'a str) -> ParseResult<'a, (&'a str, O)> {
    let mut next = alt((map(end, Some), map(anychar, |_| None)));
    move |start: &'a str| {
        let mut i = start;
        loop {
            i = match candidate_finder.split(i) {
                Some((_, j)) => j,
                None => return Err(nom::Err::Error(ErrorContext::new("`end` not found`", i))),
            };
            i = match next(i)? {
                (j, Some(lookahead)) => return Ok((i, (j, lookahead))),
                (j, None) => j,
            };
        }
    }
}

fn keyword<'a>(k: &'a str) -> impl FnMut(&'a str) -> ParseResult<'a> {
    move |i: &'a str| -> ParseResult<'a> {
        let (j, v) = identifier(i)?;
        if k == v { Ok((j, v)) } else { fail(i) }
    }
}

fn identifier(input: &str) -> ParseResult<'_> {
    fn start(s: &str) -> ParseResult<'_> {
        s.split_at_position1_complete(
            |c| !(c.is_alpha() || c == '_' || c >= '\u{0080}'),
            nom::error::ErrorKind::Alpha,
        )
    }

    fn tail(s: &str) -> ParseResult<'_> {
        s.split_at_position1_complete(
            |c| !(c.is_alphanum() || c == '_' || c >= '\u{0080}'),
            nom::error::ErrorKind::Alpha,
        )
    }

    recognize(pair(start, opt(tail)))(input)
}

fn bool_lit(i: &str) -> ParseResult<'_> {
    alt((keyword("false"), keyword("true")))(i)
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Num<'a> {
    Int(&'a str, Option<IntKind>),
    Float(&'a str, Option<FloatKind>),
}

fn num_lit<'a>(start: &'a str) -> ParseResult<'a, Num<'a>> {
    fn num_lit_suffix<'a, T: Copy>(
        kind: &'a str,
        list: &[(&str, T)],
        start: &'a str,
        i: &'a str,
    ) -> ParseResult<'a, T> {
        let (i, suffix) = identifier(i)?;
        if let Some(value) = list
            .iter()
            .copied()
            .find_map(|(name, value)| (name == suffix).then_some(value))
        {
            Ok((i, value))
        } else {
            Err(nom::Err::Failure(ErrorContext::new(
                format!("unknown {kind} suffix `{suffix}`"),
                start,
            )))
        }
    }

    // Equivalent to <https://github.com/rust-lang/rust/blob/e3f909b2bbd0b10db6f164d466db237c582d3045/compiler/rustc_lexer/src/lib.rs#L587-L620>.
    let int_with_base = pair(opt(char('-')), |i| {
        let (i, (kind, base)) = consumed(preceded(
            char('0'),
            alt((
                value(2, char('b')),
                value(8, char('o')),
                value(16, char('x')),
            )),
        ))(i)?;
        match opt(separated_digits(base, false))(i)? {
            (i, Some(_)) => Ok((i, ())),
            (_, None) => Err(nom::Err::Failure(ErrorContext::new(
                format!("expected digits after `{kind}`"),
                start,
            ))),
        }
    });

    // Equivalent to <https://github.com/rust-lang/rust/blob/e3f909b2bbd0b10db6f164d466db237c582d3045/compiler/rustc_lexer/src/lib.rs#L626-L653>:
    // no `_` directly after the decimal point `.`, or between `e` and `+/-`.
    let float = |i: &'a str| -> ParseResult<'a, ()> {
        let (i, has_dot) = opt(pair(char('.'), separated_digits(10, true)))(i)?;
        let (i, has_exp) = opt(|i| {
            let (i, (kind, op)) = pair(one_of("eE"), opt(one_of("+-")))(i)?;
            match opt(separated_digits(10, op.is_none()))(i)? {
                (i, Some(_)) => Ok((i, ())),
                (_, None) => Err(nom::Err::Failure(ErrorContext::new(
                    format!("expected decimal digits, `+` or `-` after exponent `{kind}`"),
                    start,
                ))),
            }
        })(i)?;
        match (has_dot, has_exp) {
            (Some(_), _) | (_, Some(())) => Ok((i, ())),
            _ => fail(start),
        }
    };

    let (i, num) = if let Ok((i, Some(num))) = opt(recognize(int_with_base))(start) {
        let (i, suffix) = opt(|i| num_lit_suffix("integer", INTEGER_TYPES, start, i))(i)?;
        (i, Num::Int(num, suffix))
    } else {
        let (i, (num, float)) = consumed(preceded(
            pair(opt(char('-')), separated_digits(10, true)),
            opt(float),
        ))(start)?;
        if float.is_some() {
            let (i, suffix) = opt(|i| num_lit_suffix("float", FLOAT_TYPES, start, i))(i)?;
            (i, Num::Float(num, suffix))
        } else {
            let (i, suffix) = opt(|i| num_lit_suffix("number", NUM_TYPES, start, i))(i)?;
            match suffix {
                Some(NumKind::Int(kind)) => (i, Num::Int(num, Some(kind))),
                Some(NumKind::Float(kind)) => (i, Num::Float(num, Some(kind))),
                None => (i, Num::Int(num, None)),
            }
        }
    };
    Ok((i, num))
}

/// Underscore separated digits of the given base, unless `start` is true this may start
/// with an underscore.
fn separated_digits(radix: u32, start: bool) -> impl Fn(&str) -> ParseResult<'_> {
    move |i| {
        recognize(tuple((
            |i| match start {
                true => Ok((i, 0)),
                false => many0_count(char('_'))(i),
            },
            satisfy(|ch| ch.is_digit(radix)),
            many0_count(satisfy(|ch| ch == '_' || ch.is_digit(radix))),
        )))(i)
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum StrPrefix {
    Binary,
    CLike,
}

impl StrPrefix {
    #[must_use]
    pub fn to_char(self) -> char {
        match self {
            Self::Binary => 'b',
            Self::CLike => 'c',
        }
    }
}

impl fmt::Display for StrPrefix {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use std::fmt::Write;

        f.write_char(self.to_char())
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct StrLit<'a> {
    pub prefix: Option<StrPrefix>,
    pub content: &'a str,
}

fn str_lit_without_prefix(i: &str) -> ParseResult<'_> {
    let (i, s) = delimited(
        char('"'),
        opt(escaped(is_not("\\\""), '\\', anychar)),
        char('"'),
    )(i)?;
    Ok((i, s.unwrap_or_default()))
}

fn str_lit(i: &str) -> Result<(&str, StrLit<'_>), ParseErr<'_>> {
    let (i, (prefix, content)) =
        tuple((opt(alt((char('b'), char('c')))), str_lit_without_prefix))(i)?;
    let prefix = match prefix {
        Some('b') => Some(StrPrefix::Binary),
        Some('c') => Some(StrPrefix::CLike),
        _ => None,
    };
    Ok((i, StrLit { prefix, content }))
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum CharPrefix {
    Binary,
}

#[derive(Clone, Debug, PartialEq)]
pub struct CharLit<'a> {
    pub prefix: Option<CharPrefix>,
    pub content: &'a str,
}

// Information about allowed character escapes is available at:
// <https://doc.rust-lang.org/reference/tokens.html#character-literals>.
fn char_lit(i: &str) -> Result<(&str, CharLit<'_>), ParseErr<'_>> {
    let start = i;
    let (i, (b_prefix, s)) = tuple((
        opt(char('b')),
        delimited(
            char('\''),
            opt(escaped(is_not("\\\'"), '\\', anychar)),
            char('\''),
        ),
    ))(i)?;

    let Some(s) = s else {
        return Err(nom::Err::Failure(ErrorContext::new(
            "empty character literal",
            start,
        )));
    };
    let Ok(("", c)) = Char::parse(s) else {
        return Err(nom::Err::Failure(ErrorContext::new(
            "invalid character",
            start,
        )));
    };

    let (nb, max_value, err1, err2) = match c {
        Char::Literal | Char::Escaped => {
            return Ok((i, CharLit {
                prefix: b_prefix.map(|_| CharPrefix::Binary),
                content: s,
            }));
        }
        Char::AsciiEscape(nb) => (
            nb,
            // `0x7F` is the maximum value for a `\x` escaped character.
            0x7F,
            "invalid character in ascii escape",
            "must be a character in the range [\\x00-\\x7f]",
        ),
        Char::UnicodeEscape(nb) => (
            nb,
            // `0x10FFFF` is the maximum value for a `\u` escaped character.
            0x0010_FFFF,
            "invalid character in unicode escape",
            "unicode escape must be at most 10FFFF",
        ),
    };

    let Ok(nb) = u32::from_str_radix(nb, 16) else {
        return Err(nom::Err::Failure(ErrorContext::new(err1, start)));
    };
    if nb > max_value {
        return Err(nom::Err::Failure(ErrorContext::new(err2, start)));
    }

    Ok((i, CharLit {
        prefix: b_prefix.map(|_| CharPrefix::Binary),
        content: s,
    }))
}

/// Represents the different kinds of char declarations:
enum Char<'a> {
    /// Any character that is not escaped.
    Literal,
    /// An escaped character (like `\n`) which doesn't require any extra check.
    Escaped,
    /// Ascii escape (like `\x12`).
    AsciiEscape(&'a str),
    /// Unicode escape (like `\u{12}`).
    UnicodeEscape(&'a str),
}

impl<'a> Char<'a> {
    fn parse(i: &'a str) -> ParseResult<'a, Self> {
        if i.chars().count() == 1 {
            return Ok(("", Self::Literal));
        }
        map(
            tuple((
                char('\\'),
                alt((
                    map(char('n'), |_| Self::Escaped),
                    map(char('r'), |_| Self::Escaped),
                    map(char('t'), |_| Self::Escaped),
                    map(char('\\'), |_| Self::Escaped),
                    map(char('0'), |_| Self::Escaped),
                    map(char('\''), |_| Self::Escaped),
                    // Not useful but supported by rust.
                    map(char('"'), |_| Self::Escaped),
                    map(
                        tuple((
                            char('x'),
                            take_while_m_n(2, 2, |c: char| c.is_ascii_hexdigit()),
                        )),
                        |(_, s)| Self::AsciiEscape(s),
                    ),
                    map(
                        tuple((
                            tag("u{"),
                            take_while_m_n(1, 6, |c: char| c.is_ascii_hexdigit()),
                            char('}'),
                        )),
                        |(_, s, _)| Self::UnicodeEscape(s),
                    ),
                )),
            )),
            |(_, ch)| ch,
        )(i)
    }
}

enum PathOrIdentifier<'a> {
    Path(Vec<&'a str>),
    Identifier(&'a str),
}

fn path_or_identifier(i: &str) -> ParseResult<'_, PathOrIdentifier<'_>> {
    let root = ws(opt(tag("::")));
    let tail = opt(many1(preceded(ws(tag("::")), identifier)));

    let (i, (root, start, rest)) = tuple((root, identifier, tail))(i)?;
    let rest = rest.as_deref().unwrap_or_default();

    // The returned identifier can be assumed to be path if:
    // - it is an absolute path (starts with `::`), or
    // - it has multiple components (at least one `::`), or
    // - the first letter is uppercase
    match (root, start, rest) {
        (Some(_), start, tail) => {
            let mut path = Vec::with_capacity(2 + tail.len());
            path.push("");
            path.push(start);
            path.extend(rest);
            Ok((i, PathOrIdentifier::Path(path)))
        }
        (None, name, []) if name.chars().next().map_or(true, char::is_lowercase) => {
            Ok((i, PathOrIdentifier::Identifier(name)))
        }
        (None, start, tail) => {
            let mut path = Vec::with_capacity(1 + tail.len());
            path.push(start);
            path.extend(rest);
            Ok((i, PathOrIdentifier::Path(path)))
        }
    }
}

struct State<'a> {
    syntax: &'a Syntax<'a>,
    loop_depth: Cell<usize>,
    level: Cell<Level>,
}

impl<'a> State<'a> {
    fn new(syntax: &'a Syntax<'a>) -> State<'a> {
        State {
            syntax,
            loop_depth: Cell::new(0),
            level: Cell::new(Level::default()),
        }
    }

    fn nest<'b, T, F: FnOnce(&'b str) -> ParseResult<'b, T>>(
        &self,
        i: &'b str,
        callback: F,
    ) -> ParseResult<'b, T> {
        let prev_level = self.level.get();
        let (_, level) = prev_level.nest(i)?;
        self.level.set(level);
        let ret = callback(i);
        self.level.set(prev_level);
        ret
    }

    fn tag_block_start<'i>(&self, i: &'i str) -> ParseResult<'i> {
        tag(self.syntax.block_start)(i)
    }

    fn tag_block_end<'i>(&self, i: &'i str) -> ParseResult<'i> {
        tag(self.syntax.block_end)(i)
    }

    fn tag_comment_start<'i>(&self, i: &'i str) -> ParseResult<'i> {
        tag(self.syntax.comment_start)(i)
    }

    fn tag_comment_end<'i>(&self, i: &'i str) -> ParseResult<'i> {
        tag(self.syntax.comment_end)(i)
    }

    fn tag_expr_start<'i>(&self, i: &'i str) -> ParseResult<'i> {
        tag(self.syntax.expr_start)(i)
    }

    fn tag_expr_end<'i>(&self, i: &'i str) -> ParseResult<'i> {
        tag(self.syntax.expr_end)(i)
    }

    fn enter_loop(&self) {
        self.loop_depth.set(self.loop_depth.get() + 1);
    }

    fn leave_loop(&self) {
        self.loop_depth.set(self.loop_depth.get() - 1);
    }

    fn is_in_loop(&self) -> bool {
        self.loop_depth.get() > 0
    }
}

#[derive(Default, Hash, PartialEq, Clone, Copy)]
pub struct Syntax<'a>(InnerSyntax<'a>);

// This abstraction ensures that the fields are readable, but not writable.
#[derive(Hash, PartialEq, Clone, Copy)]
pub struct InnerSyntax<'a> {
    pub block_start: &'a str,
    pub block_end: &'a str,
    pub expr_start: &'a str,
    pub expr_end: &'a str,
    pub comment_start: &'a str,
    pub comment_end: &'a str,
}

impl<'a> Deref for Syntax<'a> {
    type Target = InnerSyntax<'a>;

    #[inline]
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl Default for InnerSyntax<'static> {
    fn default() -> Self {
        Self {
            block_start: "{%",
            block_end: "%}",
            expr_start: "{{",
            expr_end: "}}",
            comment_start: "{#",
            comment_end: "#}",
        }
    }
}

impl<'a> fmt::Debug for Syntax<'a> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt_syntax("Syntax", self, f)
    }
}

impl<'a> fmt::Debug for InnerSyntax<'a> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt_syntax("InnerSyntax", self, f)
    }
}

fn fmt_syntax(name: &str, inner: &InnerSyntax<'_>, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    f.debug_struct(name)
        .field("block_start", &inner.block_start)
        .field("block_end", &inner.block_end)
        .field("expr_start", &inner.expr_start)
        .field("expr_end", &inner.expr_end)
        .field("comment_start", &inner.comment_start)
        .field("comment_end", &inner.comment_end)
        .finish()
}

#[derive(Debug, Default, Clone, Copy, Hash, PartialEq)]
#[cfg_attr(feature = "config", derive(serde::Deserialize))]
pub struct SyntaxBuilder<'a> {
    pub name: &'a str,
    pub block_start: Option<&'a str>,
    pub block_end: Option<&'a str>,
    pub expr_start: Option<&'a str>,
    pub expr_end: Option<&'a str>,
    pub comment_start: Option<&'a str>,
    pub comment_end: Option<&'a str>,
}

impl<'a> SyntaxBuilder<'a> {
    pub fn to_syntax(&self) -> Result<Syntax<'a>, String> {
        let default = InnerSyntax::default();
        let syntax = Syntax(InnerSyntax {
            block_start: self.block_start.unwrap_or(default.block_start),
            block_end: self.block_end.unwrap_or(default.block_end),
            expr_start: self.expr_start.unwrap_or(default.expr_start),
            expr_end: self.expr_end.unwrap_or(default.expr_end),
            comment_start: self.comment_start.unwrap_or(default.comment_start),
            comment_end: self.comment_end.unwrap_or(default.comment_end),
        });

        for (s, k, is_closing) in [
            (syntax.block_start, "opening block", false),
            (syntax.block_end, "closing block", true),
            (syntax.expr_start, "opening expression", false),
            (syntax.expr_end, "closing expression", true),
            (syntax.comment_start, "opening comment", false),
            (syntax.comment_end, "closing comment", true),
        ] {
            if s.len() < 2 {
                return Err(format!(
                    "delimiters must be at least two characters long. \
                        The {k} delimiter ({s:?}) is too short",
                ));
            } else if s.chars().any(char::is_whitespace) {
                return Err(format!(
                    "delimiters may not contain white spaces. \
                        The {k} delimiter ({s:?}) contains white spaces",
                ));
            } else if is_closing
                && ['(', '-', '+', '~', '.', '>', '<', '&', '|', '!']
                    .contains(&s.chars().next().unwrap())
            {
                return Err(format!(
                    "closing delimiters may not start with operators. \
                        The {k} delimiter ({s:?}) starts with operator `{}`",
                    s.chars().next().unwrap(),
                ));
            }
        }

        for ((s1, k1), (s2, k2)) in [
            (
                (syntax.block_start, "block"),
                (syntax.expr_start, "expression"),
            ),
            (
                (syntax.block_start, "block"),
                (syntax.comment_start, "comment"),
            ),
            (
                (syntax.expr_start, "expression"),
                (syntax.comment_start, "comment"),
            ),
        ] {
            if s1.starts_with(s2) || s2.starts_with(s1) {
                let (s1, k1, s2, k2) = match s1.len() < s2.len() {
                    true => (s1, k1, s2, k2),
                    false => (s2, k2, s1, k1),
                };
                return Err(format!(
                    "an opening delimiter may not be the prefix of another delimiter. \
                        The {k1} delimiter ({s1:?}) clashes with the {k2} delimiter ({s2:?})",
                ));
            }
        }

        Ok(syntax)
    }
}

#[derive(Clone, Copy, Default)]
pub(crate) struct Level(u8);

impl Level {
    fn nest(self, i: &str) -> ParseResult<'_, Level> {
        if self.0 >= Self::MAX_DEPTH {
            return Err(nom::Err::Failure(ErrorContext::new(
                "your template code is too deeply nested, or last expression is too complex",
                i,
            )));
        }

        Ok((i, Level(self.0 + 1)))
    }

    const MAX_DEPTH: u8 = 128;
}

fn filter<'a>(
    i: &'a str,
    level: &mut Level,
) -> ParseResult<'a, (&'a str, Option<Vec<WithSpan<'a, Expr<'a>>>>)> {
    let (j, _) = take_till(not_ws)(i)?;
    let had_spaces = i.len() != j.len();
    let (j, _) = pair(char('|'), not(char('|')))(j)?;

    if !had_spaces {
        *level = level.nest(i)?.1;
        cut(pair(
            ws(identifier),
            opt(|i| Expr::arguments(i, *level, false)),
        ))(j)
    } else {
        Err(nom::Err::Failure(ErrorContext::new(
            "the filter operator `|` must not be preceded by any whitespace characters\n\
            the binary OR operator is called `bitor` in rinja",
            i,
        )))
    }
}

/// Returns the common parts of two paths.
///
/// The goal of this function is to reduce the path length based on the `base` argument
/// (generally the path where the program is running into). For example:
///
/// ```text
/// current dir: /a/b/c
/// path:        /a/b/c/d/e.txt
/// ```
///
/// `strip_common` will return `d/e.txt`.
#[must_use]
pub fn strip_common(base: &Path, path: &Path) -> String {
    let path = match path.canonicalize() {
        Ok(path) => path,
        Err(_) => return path.display().to_string(),
    };
    let mut components_iter = path.components().peekable();

    for current_path_component in base.components() {
        let Some(path_component) = components_iter.peek() else {
            return path.display().to_string();
        };
        if current_path_component != *path_component {
            break;
        }
        components_iter.next();
    }
    let path_parts = components_iter
        .map(|c| c.as_os_str().to_string_lossy())
        .collect::<Vec<_>>();
    if path_parts.is_empty() {
        path.display().to_string()
    } else {
        path_parts.join(std::path::MAIN_SEPARATOR_STR)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IntKind {
    I8,
    I16,
    I32,
    I64,
    I128,
    Isize,
    U8,
    U16,
    U32,
    U64,
    U128,
    Usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FloatKind {
    F16,
    F32,
    F64,
    F128,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum NumKind {
    Int(IntKind),
    Float(FloatKind),
}

/// Primitive integer types. Also used as number suffixes.
const INTEGER_TYPES: &[(&str, IntKind)] = &[
    ("i8", IntKind::I8),
    ("i16", IntKind::I16),
    ("i32", IntKind::I32),
    ("i64", IntKind::I64),
    ("i128", IntKind::I128),
    ("isize", IntKind::Isize),
    ("u8", IntKind::U8),
    ("u16", IntKind::U16),
    ("u32", IntKind::U32),
    ("u64", IntKind::U64),
    ("u128", IntKind::U128),
    ("usize", IntKind::Usize),
];

/// Primitive floating point types. Also used as number suffixes.
const FLOAT_TYPES: &[(&str, FloatKind)] = &[
    ("f16", FloatKind::F16),
    ("f32", FloatKind::F32),
    ("f64", FloatKind::F64),
    ("f128", FloatKind::F128),
];

/// Primitive numeric types. Also used as number suffixes.
const NUM_TYPES: &[(&str, NumKind)] = &{
    let mut list = [("", NumKind::Int(IntKind::I8)); INTEGER_TYPES.len() + FLOAT_TYPES.len()];
    let mut i = 0;
    let mut o = 0;
    while i < INTEGER_TYPES.len() {
        let (name, value) = INTEGER_TYPES[i];
        list[o] = (name, NumKind::Int(value));
        i += 1;
        o += 1;
    }
    let mut i = 0;
    while i < FLOAT_TYPES.len() {
        let (name, value) = FLOAT_TYPES[i];
        list[o] = (name, NumKind::Float(value));
        i += 1;
        o += 1;
    }
    list
};

/// Complete list of named primitive types.
const PRIMITIVE_TYPES: &[&str] = &{
    let mut list = [""; NUM_TYPES.len() + 1];
    let mut i = 0;
    let mut o = 0;
    while i < NUM_TYPES.len() {
        list[o] = NUM_TYPES[i].0;
        i += 1;
        o += 1;
    }
    list[o] = "bool";
    list
};

#[cfg(not(windows))]
#[cfg(test)]
mod test {
    use std::path::Path;

    use super::*;

    #[test]
    fn test_strip_common() {
        // Full path is returned instead of empty when the entire path is in common.
        assert_eq!(strip_common(Path::new("home"), Path::new("home")), "home");

        let cwd = std::env::current_dir().expect("current_dir failed");

        // We need actual existing paths for `canonicalize` to work, so let's do that.
        let entry = cwd
            .read_dir()
            .expect("read_dir failed")
            .filter_map(std::result::Result::ok)
            .find(|f| f.path().is_file())
            .expect("no entry");

        // Since they have the complete path in common except for the folder entry name, it should
        // return only the folder entry name.
        assert_eq!(
            strip_common(&cwd, &entry.path()),
            entry.file_name().to_string_lossy()
        );

        // In this case it cannot canonicalize `/a/b/c` so it returns the path as is.
        assert_eq!(strip_common(&cwd, Path::new("/a/b/c")), "/a/b/c");
    }

    #[test]
    fn test_num_lit() {
        // Should fail.
        assert!(num_lit(".").is_err());
        // Should succeed.
        assert_eq!(
            num_lit("1.2E-02").unwrap(),
            ("", Num::Float("1.2E-02", None))
        );
        assert_eq!(num_lit("4e3").unwrap(), ("", Num::Float("4e3", None)),);
        assert_eq!(num_lit("4e+_3").unwrap(), ("", Num::Float("4e+_3", None)),);
        // Not supported because Rust wants a number before the `.`.
        assert!(num_lit(".1").is_err());
        assert!(num_lit(".1E-02").is_err());
        // A `_` directly after the `.` denotes a field.
        assert_eq!(num_lit("1._0").unwrap(), ("._0", Num::Int("1", None)));
        assert_eq!(num_lit("1_.0").unwrap(), ("", Num::Float("1_.0", None)));
        // Not supported (voluntarily because of `1..` syntax).
        assert_eq!(num_lit("1.").unwrap(), (".", Num::Int("1", None)));
        assert_eq!(num_lit("1_.").unwrap(), (".", Num::Int("1_", None)));
        assert_eq!(num_lit("1_2.").unwrap(), (".", Num::Int("1_2", None)));
        // Numbers with suffixes
        assert_eq!(
            num_lit("-1usize").unwrap(),
            ("", Num::Int("-1", Some(IntKind::Usize)))
        );
        assert_eq!(
            num_lit("123_f32").unwrap(),
            ("", Num::Float("123_", Some(FloatKind::F32)))
        );
        assert_eq!(
            num_lit("1_.2_e+_3_f64|into_isize").unwrap(),
            (
                "|into_isize",
                Num::Float("1_.2_e+_3_", Some(FloatKind::F64))
            )
        );
        assert_eq!(
            num_lit("4e3f128").unwrap(),
            ("", Num::Float("4e3", Some(FloatKind::F128))),
        );
    }

    #[test]
    fn test_char_lit() {
        let lit = |s: &'static str| crate::CharLit {
            prefix: None,
            content: s,
        };

        assert_eq!(char_lit("'a'").unwrap(), ("", lit("a")));
        assert_eq!(char_lit("'字'").unwrap(), ("", lit("字")));

        // Escaped single characters.
        assert_eq!(char_lit("'\\\"'").unwrap(), ("", lit("\\\"")));
        assert_eq!(char_lit("'\\''").unwrap(), ("", lit("\\'")));
        assert_eq!(char_lit("'\\t'").unwrap(), ("", lit("\\t")));
        assert_eq!(char_lit("'\\n'").unwrap(), ("", lit("\\n")));
        assert_eq!(char_lit("'\\r'").unwrap(), ("", lit("\\r")));
        assert_eq!(char_lit("'\\0'").unwrap(), ("", lit("\\0")));
        // Escaped ascii characters (up to `0x7F`).
        assert_eq!(char_lit("'\\x12'").unwrap(), ("", lit("\\x12")));
        assert_eq!(char_lit("'\\x02'").unwrap(), ("", lit("\\x02")));
        assert_eq!(char_lit("'\\x6a'").unwrap(), ("", lit("\\x6a")));
        assert_eq!(char_lit("'\\x7F'").unwrap(), ("", lit("\\x7F")));
        // Escaped unicode characters (up to `0x10FFFF`).
        assert_eq!(char_lit("'\\u{A}'").unwrap(), ("", lit("\\u{A}")));
        assert_eq!(char_lit("'\\u{10}'").unwrap(), ("", lit("\\u{10}")));
        assert_eq!(char_lit("'\\u{aa}'").unwrap(), ("", lit("\\u{aa}")));
        assert_eq!(char_lit("'\\u{10FFFF}'").unwrap(), ("", lit("\\u{10FFFF}")));

        // Check with `b` prefix.
        assert_eq!(
            char_lit("b'a'").unwrap(),
            ("", crate::CharLit {
                prefix: Some(crate::CharPrefix::Binary),
                content: "a"
            })
        );

        // Should fail.
        assert!(char_lit("''").is_err());
        assert!(char_lit("'\\o'").is_err());
        assert!(char_lit("'\\x'").is_err());
        assert!(char_lit("'\\x1'").is_err());
        assert!(char_lit("'\\x80'").is_err());
        assert!(char_lit("'\\u'").is_err());
        assert!(char_lit("'\\u{}'").is_err());
        assert!(char_lit("'\\u{110000}'").is_err());
    }

    #[test]
    fn test_str_lit() {
        assert_eq!(
            str_lit(r#"b"hello""#).unwrap(),
            ("", StrLit {
                prefix: Some(StrPrefix::Binary),
                content: "hello"
            })
        );
        assert_eq!(
            str_lit(r#"c"hello""#).unwrap(),
            ("", StrLit {
                prefix: Some(StrPrefix::CLike),
                content: "hello"
            })
        );
        assert!(str_lit(r#"d"hello""#).is_err());
    }
}
