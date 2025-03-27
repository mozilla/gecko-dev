use std::collections::HashSet;
use std::str;

use nom::branch::alt;
use nom::bytes::complete::{tag, take_till};
use nom::character::complete::{char, digit1};
use nom::combinator::{consumed, cut, fail, map, not, opt, peek, recognize, value};
use nom::error::ErrorKind;
use nom::error_position;
use nom::multi::{fold_many0, many0, separated_list0, separated_list1};
use nom::sequence::{pair, preceded, terminated, tuple};

use crate::{
    CharLit, ErrorContext, Level, Num, ParseResult, PathOrIdentifier, StrLit, WithSpan, char_lit,
    filter, identifier, keyword, not_ws, num_lit, path_or_identifier, str_lit, ws,
};

macro_rules! expr_prec_layer {
    ( $name:ident, $inner:ident, $op:expr ) => {
        fn $name(i: &'a str, level: Level) -> ParseResult<'a, WithSpan<'a, Self>> {
            let (_, level) = level.nest(i)?;
            let start = i;
            let (i, left) = Self::$inner(i, level)?;
            let (i, right) = many0(pair(ws($op), |i| Self::$inner(i, level)))(i)?;
            Ok((
                i,
                right.into_iter().fold(left, |left, (op, right)| {
                    WithSpan::new(Self::BinOp(op, Box::new(left), Box::new(right)), start)
                }),
            ))
        }
    };
}

#[derive(Clone, Debug, PartialEq)]
pub enum Expr<'a> {
    BoolLit(bool),
    NumLit(&'a str, Num<'a>),
    StrLit(StrLit<'a>),
    CharLit(CharLit<'a>),
    Var(&'a str),
    Path(Vec<&'a str>),
    Array(Vec<WithSpan<'a, Expr<'a>>>),
    Attr(Box<WithSpan<'a, Expr<'a>>>, &'a str),
    Index(Box<WithSpan<'a, Expr<'a>>>, Box<WithSpan<'a, Expr<'a>>>),
    Filter(Filter<'a>),
    As(Box<WithSpan<'a, Expr<'a>>>, &'a str),
    NamedArgument(&'a str, Box<WithSpan<'a, Expr<'a>>>),
    Unary(&'a str, Box<WithSpan<'a, Expr<'a>>>),
    BinOp(
        &'a str,
        Box<WithSpan<'a, Expr<'a>>>,
        Box<WithSpan<'a, Expr<'a>>>,
    ),
    Range(
        &'a str,
        Option<Box<WithSpan<'a, Expr<'a>>>>,
        Option<Box<WithSpan<'a, Expr<'a>>>>,
    ),
    Group(Box<WithSpan<'a, Expr<'a>>>),
    Tuple(Vec<WithSpan<'a, Expr<'a>>>),
    Call(Box<WithSpan<'a, Expr<'a>>>, Vec<WithSpan<'a, Expr<'a>>>),
    RustMacro(Vec<&'a str>, &'a str),
    Try(Box<WithSpan<'a, Expr<'a>>>),
    /// This variant should never be used directly. It is created when generating filter blocks.
    FilterSource,
    IsDefined(&'a str),
    IsNotDefined(&'a str),
}

impl<'a> Expr<'a> {
    pub(super) fn arguments(
        i: &'a str,
        level: Level,
        is_template_macro: bool,
    ) -> ParseResult<'a, Vec<WithSpan<'a, Self>>> {
        let (_, level) = level.nest(i)?;
        let mut named_arguments = HashSet::new();
        let start = i;

        preceded(
            ws(char('(')),
            cut(terminated(
                separated_list0(
                    char(','),
                    ws(move |i| {
                        // Needed to prevent borrowing it twice between this closure and the one
                        // calling `Self::named_arguments`.
                        let named_arguments = &mut named_arguments;
                        let has_named_arguments = !named_arguments.is_empty();

                        let (i, expr) = alt((
                            move |i| {
                                Self::named_argument(
                                    i,
                                    level,
                                    named_arguments,
                                    start,
                                    is_template_macro,
                                )
                            },
                            move |i| Self::parse(i, level),
                        ))(i)?;
                        if has_named_arguments && !matches!(*expr, Self::NamedArgument(_, _)) {
                            Err(nom::Err::Failure(ErrorContext::new(
                                "named arguments must always be passed last",
                                start,
                            )))
                        } else {
                            Ok((i, expr))
                        }
                    }),
                ),
                tuple((opt(ws(char(','))), char(')'))),
            )),
        )(i)
    }

    fn named_argument(
        i: &'a str,
        level: Level,
        named_arguments: &mut HashSet<&'a str>,
        start: &'a str,
        is_template_macro: bool,
    ) -> ParseResult<'a, WithSpan<'a, Self>> {
        if !is_template_macro {
            // If this is not a template macro, we don't want to parse named arguments so
            // we instead return an error which will allow to continue the parsing.
            return fail(i);
        }

        let (_, level) = level.nest(i)?;
        let (i, (argument, _, value)) =
            tuple((identifier, ws(char('=')), move |i| Self::parse(i, level)))(i)?;
        if named_arguments.insert(argument) {
            Ok((
                i,
                WithSpan::new(Self::NamedArgument(argument, Box::new(value)), start),
            ))
        } else {
            Err(nom::Err::Failure(ErrorContext::new(
                format!("named argument `{argument}` was passed more than once"),
                start,
            )))
        }
    }

    pub(super) fn parse(i: &'a str, level: Level) -> ParseResult<'a, WithSpan<'a, Self>> {
        let (_, level) = level.nest(i)?;
        let start = i;
        let range_right = move |i| {
            pair(
                ws(alt((tag("..="), tag("..")))),
                opt(move |i| Self::or(i, level)),
            )(i)
        };
        alt((
            map(range_right, |(op, right)| {
                WithSpan::new(Self::Range(op, None, right.map(Box::new)), start)
            }),
            map(
                pair(move |i| Self::or(i, level), opt(range_right)),
                |(left, right)| match right {
                    Some((op, right)) => WithSpan::new(
                        Self::Range(op, Some(Box::new(left)), right.map(Box::new)),
                        start,
                    ),
                    None => left,
                },
            ),
        ))(i)
    }

    expr_prec_layer!(or, and, tag("||"));
    expr_prec_layer!(and, compare, tag("&&"));
    expr_prec_layer!(
        compare,
        bor,
        alt((
            tag("=="),
            tag("!="),
            tag(">="),
            tag(">"),
            tag("<="),
            tag("<"),
        ))
    );
    expr_prec_layer!(bor, bxor, value("|", tag("bitor")));
    expr_prec_layer!(bxor, band, token_xor);
    expr_prec_layer!(band, shifts, token_bitand);
    expr_prec_layer!(shifts, addsub, alt((tag(">>"), tag("<<"))));
    expr_prec_layer!(addsub, muldivmod, alt((tag("+"), tag("-"))));
    expr_prec_layer!(muldivmod, is_as, alt((tag("*"), tag("/"), tag("%"))));

    fn is_as(i: &'a str, level: Level) -> ParseResult<'a, WithSpan<'a, Self>> {
        let start = i;
        let (before_keyword, lhs) = Self::filtered(i, level)?;
        let (j, rhs) = opt(ws(identifier))(before_keyword)?;
        let i = match rhs {
            Some("is") => j,
            Some("as") => {
                let (i, target) = opt(identifier)(j)?;
                let target = target.unwrap_or_default();
                if crate::PRIMITIVE_TYPES.contains(&target) {
                    return Ok((i, WithSpan::new(Self::As(Box::new(lhs), target), start)));
                } else if target.is_empty() {
                    return Err(nom::Err::Failure(ErrorContext::new(
                        "`as` operator expects the name of a primitive type on its right-hand side",
                        before_keyword.trim_start(),
                    )));
                } else {
                    return Err(nom::Err::Failure(ErrorContext::new(
                        format!(
                            "`as` operator expects the name of a primitive type on its right-hand \
                              side, found `{target}`"
                        ),
                        before_keyword.trim_start(),
                    )));
                }
            }
            _ => return Ok((before_keyword, lhs)),
        };

        let (i, rhs) = opt(terminated(opt(keyword("not")), ws(keyword("defined"))))(i)?;
        let ctor = match rhs {
            None => {
                return Err(nom::Err::Failure(ErrorContext::new(
                    "expected `defined` or `not defined` after `is`",
                    // We use `start` to show the whole `var is` thing instead of the current token.
                    start,
                )));
            }
            Some(None) => Self::IsDefined,
            Some(Some(_)) => Self::IsNotDefined,
        };
        let var_name = match *lhs {
            Self::Var(var_name) => var_name,
            Self::Attr(_, _) => {
                return Err(nom::Err::Failure(ErrorContext::new(
                    "`is defined` operator can only be used on variables, not on their fields",
                    start,
                )));
            }
            _ => {
                return Err(nom::Err::Failure(ErrorContext::new(
                    "`is defined` operator can only be used on variables",
                    start,
                )));
            }
        };
        Ok((i, WithSpan::new(ctor(var_name), start)))
    }

    fn filtered(i: &'a str, mut level: Level) -> ParseResult<'a, WithSpan<'a, Self>> {
        let start = i;
        let (mut i, mut res) = Self::prefix(i, level)?;
        while let (j, Some((name, args))) = opt(|i| filter(i, &mut level))(i)? {
            i = j;

            let mut arguments = args.unwrap_or_else(|| Vec::with_capacity(1));
            arguments.insert(0, res);

            res = WithSpan::new(Self::Filter(Filter { name, arguments }), start);
        }
        Ok((i, res))
    }

    fn prefix(i: &'a str, mut level: Level) -> ParseResult<'a, WithSpan<'a, Self>> {
        let (_, nested) = level.nest(i)?;
        let start = i;
        let (i, (ops, mut expr)) = pair(
            many0(ws(alt((tag("!"), tag("-"), tag("*"), tag("&"))))),
            |i| Suffix::parse(i, nested),
        )(i)?;

        for op in ops.iter().rev() {
            // This is a rare place where we create recursion in the parsed AST
            // without recursing the parser call stack. However, this can lead
            // to stack overflows in drop glue when the AST is very deep.
            level = level.nest(i)?.1;
            expr = WithSpan::new(Self::Unary(op, Box::new(expr)), start);
        }

        Ok((i, expr))
    }

    fn single(i: &'a str, level: Level) -> ParseResult<'a, WithSpan<'a, Self>> {
        let (_, level) = level.nest(i)?;
        alt((
            Self::num,
            Self::str,
            Self::char,
            Self::path_var_bool,
            move |i| Self::array(i, level),
            move |i| Self::group(i, level),
        ))(i)
    }

    fn group(i: &'a str, level: Level) -> ParseResult<'a, WithSpan<'a, Self>> {
        let (_, level) = level.nest(i)?;
        let start = i;
        let (i, expr) = preceded(ws(char('(')), opt(|i| Self::parse(i, level)))(i)?;
        let Some(expr) = expr else {
            let (i, _) = char(')')(i)?;
            return Ok((i, WithSpan::new(Self::Tuple(vec![]), start)));
        };

        let (i, comma) = ws(opt(peek(char(','))))(i)?;
        if comma.is_none() {
            let (i, _) = char(')')(i)?;
            return Ok((i, WithSpan::new(Self::Group(Box::new(expr)), start)));
        }

        let mut exprs = vec![expr];
        let (i, ()) = fold_many0(
            preceded(char(','), ws(|i| Self::parse(i, level))),
            || (),
            |(), expr| {
                exprs.push(expr);
            },
        )(i)?;
        let (i, _) = pair(ws(opt(char(','))), char(')'))(i)?;
        Ok((i, WithSpan::new(Self::Tuple(exprs), start)))
    }

    fn array(i: &'a str, level: Level) -> ParseResult<'a, WithSpan<'a, Self>> {
        let start = i;
        let (i, level) = level.nest(i)?;
        let (i, array) = preceded(
            ws(char('[')),
            cut(terminated(
                opt(terminated(
                    separated_list1(char(','), ws(move |i| Self::parse(i, level))),
                    ws(opt(char(','))),
                )),
                char(']'),
            )),
        )(i)?;
        Ok((
            i,
            WithSpan::new(Self::Array(array.unwrap_or_default()), start),
        ))
    }

    fn path_var_bool(i: &'a str) -> ParseResult<'a, WithSpan<'a, Self>> {
        let start = i;
        map(path_or_identifier, |v| match v {
            PathOrIdentifier::Path(v) => Self::Path(v),
            PathOrIdentifier::Identifier("true") => Self::BoolLit(true),
            PathOrIdentifier::Identifier("false") => Self::BoolLit(false),
            PathOrIdentifier::Identifier(v) => Self::Var(v),
        })(i)
        .map(|(i, expr)| (i, WithSpan::new(expr, start)))
    }

    fn str(i: &'a str) -> ParseResult<'a, WithSpan<'a, Self>> {
        let start = i;
        map(str_lit, |i| WithSpan::new(Self::StrLit(i), start))(i)
    }

    fn num(i: &'a str) -> ParseResult<'a, WithSpan<'a, Self>> {
        let start = i;
        let (i, (full, num)) = consumed(num_lit)(i)?;
        Ok((i, WithSpan::new(Expr::NumLit(full, num), start)))
    }

    fn char(i: &'a str) -> ParseResult<'a, WithSpan<'a, Self>> {
        let start = i;
        map(char_lit, |i| WithSpan::new(Self::CharLit(i), start))(i)
    }

    #[must_use]
    pub fn contains_bool_lit_or_is_defined(&self) -> bool {
        match self {
            Self::BoolLit(_) | Self::IsDefined(_) | Self::IsNotDefined(_) => true,
            Self::Unary(_, expr) | Self::Group(expr) => expr.contains_bool_lit_or_is_defined(),
            Self::BinOp("&&" | "||", left, right) => {
                left.contains_bool_lit_or_is_defined() || right.contains_bool_lit_or_is_defined()
            }
            Self::NumLit(_, _)
            | Self::StrLit(_)
            | Self::CharLit(_)
            | Self::Var(_)
            | Self::FilterSource
            | Self::RustMacro(_, _)
            | Self::As(_, _)
            | Self::Call(_, _)
            | Self::Range(_, _, _)
            | Self::Try(_)
            | Self::NamedArgument(_, _)
            | Self::Filter(_)
            | Self::Attr(_, _)
            | Self::Index(_, _)
            | Self::Tuple(_)
            | Self::Array(_)
            | Self::BinOp(_, _, _)
            | Self::Path(_) => false,
        }
    }
}

fn token_xor(i: &str) -> ParseResult<'_> {
    let (i, good) = alt((value(true, keyword("xor")), value(false, char('^'))))(i)?;
    if good {
        Ok((i, "^"))
    } else {
        Err(nom::Err::Failure(ErrorContext::new(
            "the binary XOR operator is called `xor` in rinja",
            i,
        )))
    }
}

fn token_bitand(i: &str) -> ParseResult<'_> {
    let (i, good) = alt((
        value(true, keyword("bitand")),
        value(false, pair(char('&'), not(char('&')))),
    ))(i)?;
    if good {
        Ok((i, "&"))
    } else {
        Err(nom::Err::Failure(ErrorContext::new(
            "the binary AND operator is called `bitand` in rinja",
            i,
        )))
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Filter<'a> {
    pub name: &'a str,
    pub arguments: Vec<WithSpan<'a, Expr<'a>>>,
}

enum Suffix<'a> {
    Attr(&'a str),
    Index(WithSpan<'a, Expr<'a>>),
    Call(Vec<WithSpan<'a, Expr<'a>>>),
    // The value is the arguments of the macro call.
    MacroCall(&'a str),
    Try,
}

impl<'a> Suffix<'a> {
    fn parse(i: &'a str, level: Level) -> ParseResult<'a, WithSpan<'a, Expr<'a>>> {
        let (_, level) = level.nest(i)?;
        let (mut i, mut expr) = Expr::single(i, level)?;
        loop {
            let (j, suffix) = opt(alt((
                Self::attr,
                |i| Self::index(i, level),
                |i| Self::call(i, level),
                Self::r#try,
                Self::r#macro,
            )))(i)?;

            match suffix {
                Some(Self::Attr(attr)) => expr = WithSpan::new(Expr::Attr(expr.into(), attr), i),
                Some(Self::Index(index)) => {
                    expr = WithSpan::new(Expr::Index(expr.into(), index.into()), i);
                }
                Some(Self::Call(args)) => expr = WithSpan::new(Expr::Call(expr.into(), args), i),
                Some(Self::Try) => expr = WithSpan::new(Expr::Try(expr.into()), i),
                Some(Self::MacroCall(args)) => match expr.inner {
                    Expr::Path(path) => expr = WithSpan::new(Expr::RustMacro(path, args), i),
                    Expr::Var(name) => expr = WithSpan::new(Expr::RustMacro(vec![name], args), i),
                    _ => return Err(nom::Err::Failure(error_position!(i, ErrorKind::Tag))),
                },
                None => break,
            }

            i = j;
        }
        Ok((i, expr))
    }

    fn r#macro(i: &'a str) -> ParseResult<'a, Self> {
        fn nested_parenthesis(input: &str) -> ParseResult<'_, ()> {
            let mut nested = 0;
            let mut last = 0;
            let mut in_str = false;
            let mut escaped = false;

            for (i, c) in input.char_indices() {
                if !(c == '(' || c == ')') || !in_str {
                    match c {
                        '(' => nested += 1,
                        ')' => {
                            if nested == 0 {
                                last = i;
                                break;
                            }
                            nested -= 1;
                        }
                        '"' => {
                            if in_str {
                                if !escaped {
                                    in_str = false;
                                }
                            } else {
                                in_str = true;
                            }
                        }
                        '\\' => {
                            escaped = !escaped;
                        }
                        _ => (),
                    }
                }

                if escaped && c != '\\' {
                    escaped = false;
                }
            }

            if nested == 0 {
                Ok((&input[last..], ()))
            } else {
                fail(input)
            }
        }

        preceded(
            pair(ws(char('!')), char('(')),
            cut(terminated(
                map(recognize(nested_parenthesis), Self::MacroCall),
                char(')'),
            )),
        )(i)
    }

    fn attr(i: &'a str) -> ParseResult<'a, Self> {
        map(
            preceded(
                ws(pair(char('.'), not(char('.')))),
                cut(alt((digit1, identifier))),
            ),
            Self::Attr,
        )(i)
    }

    fn index(i: &'a str, level: Level) -> ParseResult<'a, Self> {
        let (_, level) = level.nest(i)?;
        map(
            preceded(
                ws(char('[')),
                cut(terminated(ws(move |i| Expr::parse(i, level)), char(']'))),
            ),
            Self::Index,
        )(i)
    }

    fn call(i: &'a str, level: Level) -> ParseResult<'a, Self> {
        let (_, level) = level.nest(i)?;
        map(move |i| Expr::arguments(i, level, false), Self::Call)(i)
    }

    fn r#try(i: &'a str) -> ParseResult<'a, Self> {
        map(preceded(take_till(not_ws), char('?')), |_| Self::Try)(i)
    }
}
