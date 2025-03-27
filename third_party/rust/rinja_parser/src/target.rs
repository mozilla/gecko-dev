use nom::branch::alt;
use nom::bytes::complete::tag;
use nom::character::complete::{char, one_of};
use nom::combinator::{consumed, map, map_res, opt};
use nom::multi::separated_list1;
use nom::sequence::{pair, preceded, tuple};

use crate::{
    CharLit, ErrorContext, Num, ParseErr, ParseResult, PathOrIdentifier, State, StrLit, WithSpan,
    bool_lit, char_lit, identifier, keyword, num_lit, path_or_identifier, str_lit, ws,
};

#[derive(Clone, Debug, PartialEq)]
pub enum Target<'a> {
    Name(&'a str),
    Tuple(Vec<&'a str>, Vec<Target<'a>>),
    Array(Vec<&'a str>, Vec<Target<'a>>),
    Struct(Vec<&'a str>, Vec<(&'a str, Target<'a>)>),
    NumLit(&'a str, Num<'a>),
    StrLit(StrLit<'a>),
    CharLit(CharLit<'a>),
    BoolLit(&'a str),
    Path(Vec<&'a str>),
    OrChain(Vec<Target<'a>>),
    Placeholder(&'a str),
    /// The `Option` is the variable name (if any) in `var_name @ ..`.
    Rest(WithSpan<'a, Option<&'a str>>),
}

impl<'a> Target<'a> {
    /// Parses multiple targets with `or` separating them
    pub(super) fn parse(i: &'a str, s: &State<'_>) -> ParseResult<'a, Self> {
        map(
            separated_list1(ws(tag("or")), |i| s.nest(i, |i| Self::parse_one(i, s))),
            |mut opts| match opts.len() {
                1 => opts.pop().unwrap(),
                _ => Self::OrChain(opts),
            },
        )(i)
    }

    /// Parses a single target without an `or`, unless it is wrapped in parentheses.
    fn parse_one(i: &'a str, s: &State<'_>) -> ParseResult<'a, Self> {
        let mut opt_opening_paren = map(opt(ws(char('('))), |o| o.is_some());
        let mut opt_opening_brace = map(opt(ws(char('{'))), |o| o.is_some());
        let mut opt_opening_bracket = map(opt(ws(char('['))), |o| o.is_some());

        let (i, lit) = opt(Self::lit)(i)?;
        if let Some(lit) = lit {
            return Ok((i, lit));
        }

        // match tuples and unused parentheses
        let (i, target_is_tuple) = opt_opening_paren(i)?;
        if target_is_tuple {
            let (i, (singleton, mut targets)) = collect_targets(i, s, ')', Self::unnamed)?;
            if singleton {
                return Ok((i, targets.pop().unwrap()));
            }
            return Ok((
                i,
                Self::Tuple(Vec::new(), only_one_rest_pattern(targets, false, "tuple")?),
            ));
        }
        let (i, target_is_array) = opt_opening_bracket(i)?;
        if target_is_array {
            let (i, (singleton, mut targets)) = collect_targets(i, s, ']', Self::unnamed)?;
            if singleton {
                return Ok((i, targets.pop().unwrap()));
            }
            return Ok((
                i,
                Self::Array(Vec::new(), only_one_rest_pattern(targets, true, "array")?),
            ));
        }

        let path = |i| {
            map_res(path_or_identifier, |v| match v {
                PathOrIdentifier::Path(v) => Ok(v),
                PathOrIdentifier::Identifier(v) => Err(v),
            })(i)
        };

        // match structs
        let (i, path) = opt(path)(i)?;
        if let Some(path) = path {
            let i_before_matching_with = i;
            let (i, _) = opt(ws(keyword("with")))(i)?;

            let (i, is_unnamed_struct) = opt_opening_paren(i)?;
            if is_unnamed_struct {
                let (i, (_, targets)) = collect_targets(i, s, ')', Self::unnamed)?;
                return Ok((
                    i,
                    Self::Tuple(path, only_one_rest_pattern(targets, false, "struct")?),
                ));
            }

            let (i, is_named_struct) = opt_opening_brace(i)?;
            if is_named_struct {
                let (i, (_, targets)) = collect_targets(i, s, '}', Self::named)?;
                return Ok((i, Self::Struct(path, targets)));
            }

            return Ok((i_before_matching_with, Self::Path(path)));
        }

        // neither literal nor struct nor path
        let (new_i, name) = identifier(i)?;
        let target = match name {
            "_" => Self::Placeholder(name),
            _ => verify_name(i, name)?,
        };
        Ok((new_i, target))
    }

    fn lit(i: &'a str) -> ParseResult<'a, Self> {
        alt((
            map(str_lit, Self::StrLit),
            map(char_lit, Self::CharLit),
            map(consumed(num_lit), |(full, num)| Target::NumLit(full, num)),
            map(bool_lit, Self::BoolLit),
        ))(i)
    }

    fn unnamed(i: &'a str, s: &State<'_>) -> ParseResult<'a, Self> {
        alt((Self::rest, |i| Self::parse(i, s)))(i)
    }

    fn named(init_i: &'a str, s: &State<'_>) -> ParseResult<'a, (&'a str, Self)> {
        let (i, rest) = opt(consumed(Self::rest))(init_i)?;
        if let Some(rest) = rest {
            let (_, chr) = ws(opt(one_of(",:")))(i)?;
            if let Some(chr) = chr {
                return Err(nom::Err::Failure(ErrorContext::new(
                    format!(
                        "unexpected `{chr}` character after `..`\n\
                         note that in a named struct, `..` must come last to ignore other members"
                    ),
                    i,
                )));
            }
            if let Target::Rest(ref s) = rest.1 {
                if s.inner.is_some() {
                    return Err(nom::Err::Failure(ErrorContext::new(
                        "`@ ..` cannot be used in struct",
                        s.span,
                    )));
                }
            }
            return Ok((i, rest));
        }

        let (i, (src, target)) = pair(
            identifier,
            opt(preceded(ws(char(':')), |i| Self::parse(i, s))),
        )(init_i)?;

        if src == "_" {
            return Err(nom::Err::Failure(ErrorContext::new(
                "cannot use placeholder `_` as source in named struct",
                init_i,
            )));
        }

        let target = match target {
            Some(target) => target,
            None => verify_name(init_i, src)?,
        };
        Ok((i, (src, target)))
    }

    fn rest(start: &'a str) -> ParseResult<'a, Self> {
        let (i, (ident, _)) = tuple((opt(tuple((identifier, ws(char('@'))))), tag("..")))(start)?;
        Ok((
            i,
            Self::Rest(WithSpan::new(ident.map(|(ident, _)| ident), start)),
        ))
    }
}

fn verify_name<'a>(
    input: &'a str,
    name: &'a str,
) -> Result<Target<'a>, nom::Err<ErrorContext<'a>>> {
    match name {
        "self" | "writer" => Err(nom::Err::Failure(ErrorContext::new(
            format!("cannot use `{name}` as a name"),
            input,
        ))),
        _ => Ok(Target::Name(name)),
    }
}

fn collect_targets<'a, T>(
    i: &'a str,
    s: &State<'_>,
    delim: char,
    mut one: impl FnMut(&'a str, &State<'_>) -> ParseResult<'a, T>,
) -> ParseResult<'a, (bool, Vec<T>)> {
    let opt_comma = |i| map(ws(opt(char(','))), |o| o.is_some())(i);
    let opt_end = |i| map(ws(opt(char(delim))), |o| o.is_some())(i);

    let (i, has_end) = opt_end(i)?;
    if has_end {
        return Ok((i, (false, Vec::new())));
    }

    let (i, targets) = opt(separated_list1(ws(char(',')), |i| one(i, s)))(i)?;
    let Some(targets) = targets else {
        return Err(nom::Err::Failure(ErrorContext::new(
            "expected comma separated list of members",
            i,
        )));
    };

    let (i, (has_comma, has_end)) = pair(opt_comma, opt_end)(i)?;
    if !has_end {
        let msg = match has_comma {
            true => format!("expected member, or `{delim}` as terminator"),
            false => format!("expected `,` for more members, or `{delim}` as terminator"),
        };
        return Err(nom::Err::Failure(ErrorContext::new(msg, i)));
    }

    let singleton = !has_comma && targets.len() == 1;
    Ok((i, (singleton, targets)))
}

fn only_one_rest_pattern<'a>(
    targets: Vec<Target<'a>>,
    allow_named_rest: bool,
    type_kind: &str,
) -> Result<Vec<Target<'a>>, ParseErr<'a>> {
    let mut found_rest = false;

    for target in &targets {
        if let Target::Rest(s) = target {
            if !allow_named_rest && s.inner.is_some() {
                return Err(nom::Err::Failure(ErrorContext::new(
                    "`@ ..` is only allowed in slice patterns",
                    s.span,
                )));
            }
            if found_rest {
                return Err(nom::Err::Failure(ErrorContext::new(
                    format!("`..` can only be used once per {type_kind} pattern"),
                    s.span,
                )));
            }
            found_rest = true;
        }
    }
    Ok(targets)
}
