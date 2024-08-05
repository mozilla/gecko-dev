// Copyright 2015 Ted Mielczarek. See the COPYRIGHT
// file at the top-level directory of this distribution.

use nom::branch::alt;
use nom::bytes::complete::{tag, take_while};
use nom::character::complete::{hex_digit1, space1};
use nom::character::{is_digit, is_hex_digit};
use nom::combinator::{cut, map, map_res, opt};
use nom::error::{Error, ErrorKind, ParseError};
use nom::multi::separated_list1;
use nom::sequence::{preceded, terminated, tuple};
use nom::{Err, IResult};
use range_map::{Range, RangeMap};
use tracing::warn;

use std::collections::HashMap;
use std::convert::TryFrom;
use std::fmt::Debug;
use std::{mem, str};

use minidump_common::traits::IntoRangeMapSafe;

use crate::sym_file::types::*;
use crate::SymbolError;

#[derive(Debug)]
enum Line {
    Module(String, String, String, String),
    Info(Info),
    File(u32, String),
    InlineOrigin(u32, String),
    Public(PublicSymbol),
    Function(Function, Vec<SourceLine>, Vec<Inlinee>),
    StackWin(WinFrameType),
    StackCfi(StackInfoCfi),
}

/// Match a hex string, parse it to a u32 or a u64.
fn hex_str<T: std::ops::Shl<T, Output = T> + std::ops::BitOr<T, Output = T> + From<u8>>(
    input: &[u8],
) -> IResult<&[u8], T> {
    // Consume up to max_len digits. For u32 that's 8 digits and for u64 that's 16 digits.
    // Two hex digits form one byte.
    let max_len = mem::size_of::<T>() * 2;

    let mut res: T = T::from(0);
    let mut k = 0;
    for v in input.iter().take(max_len) {
        let digit = match (*v as char).to_digit(16) {
            Some(v) => v,
            None => break,
        };
        res = res << T::from(4);
        res = res | T::from(digit as u8);
        k += 1;
    }
    if k == 0 {
        return Err(Err::Error(Error::from_error_kind(
            input,
            ErrorKind::HexDigit,
        )));
    }
    let remaining = &input[k..];
    Ok((remaining, res))
}

/// Match a decimal string, parse it to a u32.
///
/// This is doing everything manually so that we only look at each byte once.
/// With a naive implementation you might be looking at them three times: First
/// you might get a slice of acceptable characters from nom, then you might parse
/// that slice into a str (checking for utf-8 unnecessarily), and then you might
/// parse that string into a decimal number.
fn decimal_u32(input: &[u8]) -> IResult<&[u8], u32> {
    const MAX_LEN: usize = 10; // u32::MAX has 10 decimal digits
    let mut res: u64 = 0;
    let mut k = 0;
    for v in input.iter().take(MAX_LEN) {
        let digit = *v as char;
        let digit_value = match digit.to_digit(10) {
            Some(v) => v,
            None => break,
        };
        res = res * 10 + digit_value as u64;
        k += 1;
    }
    if k == 0 {
        return Err(Err::Error(Error::from_error_kind(input, ErrorKind::Digit)));
    }
    let res = u32::try_from(res)
        .map_err(|_| Err::Error(Error::from_error_kind(input, ErrorKind::TooLarge)))?;
    let remaining = &input[k..];
    Ok((remaining, res))
}

/// Take 0 or more non-space bytes.
fn non_space(input: &[u8]) -> IResult<&[u8], &[u8]> {
    take_while(|c: u8| c != b' ')(input)
}

/// Accept `\n` with an arbitrary number of preceding `\r` bytes.
///
/// This is different from `line_ending` which doesn't accept `\r` if it isn't
/// followed by `\n`.
fn my_eol(input: &[u8]) -> IResult<&[u8], &[u8]> {
    preceded(take_while(|b| b == b'\r'), tag(b"\n"))(input)
}

/// Accept everything except `\r` and `\n`.
///
/// This is different from `not_line_ending` which rejects its input if it's
/// followed by a `\r` which is not immediately followed by a `\n`.
fn not_my_eol(input: &[u8]) -> IResult<&[u8], &[u8]> {
    take_while(|b| b != b'\r' && b != b'\n')(input)
}

/// Parse a single byte if it matches the predicate.
///
/// nom has `satisfy`, which is similar. It differs in the argument type of the
/// predicate: `satisfy`'s predicate takes `char`, whereas `single`'s predicate
/// takes `u8`.
fn single(predicate: fn(u8) -> bool) -> impl Fn(&[u8]) -> IResult<&[u8], u8> {
    move |i: &[u8]| match i.split_first() {
        Some((b, rest)) if predicate(*b) => Ok((rest, *b)),
        _ => Err(Err::Error(Error::from_error_kind(i, ErrorKind::Satisfy))),
    }
}

// Matches a MODULE record.
fn module_line(input: &[u8]) -> IResult<&[u8], (String, String, String, String)> {
    let (input, _) = terminated(tag("MODULE"), space1)(input)?;
    let (input, (os, cpu, debug_id, filename)) = cut(tuple((
        terminated(map_res(non_space, str::from_utf8), space1), // os
        terminated(map_res(non_space, str::from_utf8), space1), // cpu
        terminated(map_res(hex_digit1, str::from_utf8), space1), // debug id
        terminated(map_res(not_my_eol, str::from_utf8), my_eol), // filename
    )))(input)?;
    Ok((
        input,
        (
            os.to_string(),
            cpu.to_string(),
            debug_id.to_string(),
            filename.to_string(),
        ),
    ))
}

// Matches an INFO URL record.
fn info_url(input: &[u8]) -> IResult<&[u8], Info> {
    let (input, _) = terminated(tag("INFO URL"), space1)(input)?;
    let (input, url) = cut(terminated(map_res(not_my_eol, str::from_utf8), my_eol))(input)?;
    Ok((input, Info::Url(url.to_string())))
}

// Matches other INFO records.
fn info_line(input: &[u8]) -> IResult<&[u8], &[u8]> {
    let (input, _) = terminated(tag("INFO"), space1)(input)?;
    cut(terminated(not_my_eol, my_eol))(input)
}

// Matches a FILE record.
fn file_line(input: &[u8]) -> IResult<&[u8], (u32, String)> {
    let (input, _) = terminated(tag("FILE"), space1)(input)?;
    let (input, (id, filename)) = cut(tuple((
        terminated(decimal_u32, space1),
        terminated(map_res(not_my_eol, str::from_utf8), my_eol),
    )))(input)?;
    Ok((input, (id, filename.to_string())))
}

// Matches an INLINE_ORIGIN record.
fn inline_origin_line(input: &[u8]) -> IResult<&[u8], (u32, String)> {
    let (input, _) = terminated(tag("INLINE_ORIGIN"), space1)(input)?;
    let (input, (id, function)) = cut(tuple((
        terminated(decimal_u32, space1),
        terminated(map_res(not_my_eol, str::from_utf8), my_eol),
    )))(input)?;
    Ok((input, (id, function.to_string())))
}

// Matches a PUBLIC record.
fn public_line(input: &[u8]) -> IResult<&[u8], PublicSymbol> {
    let (input, _) = terminated(tag("PUBLIC"), space1)(input)?;
    let (input, (_multiple, address, parameter_size, name)) = cut(tuple((
        opt(terminated(tag("m"), space1)),
        terminated(hex_str::<u64>, space1),
        terminated(hex_str::<u32>, space1),
        terminated(map_res(not_my_eol, str::from_utf8), my_eol),
    )))(input)?;
    Ok((
        input,
        PublicSymbol {
            address,
            parameter_size,
            name: name.to_string(),
        },
    ))
}

// Matches line data after a FUNC record.
fn func_line_data(input: &[u8]) -> IResult<&[u8], SourceLine> {
    let (input, (address, size, line, file)) = tuple((
        terminated(hex_str::<u64>, space1),
        terminated(hex_str::<u32>, space1),
        terminated(decimal_u32, space1),
        terminated(decimal_u32, my_eol),
    ))(input)?;
    Ok((
        input,
        SourceLine {
            address,
            size,
            file,
            line,
        },
    ))
}

// Matches a FUNC record.
fn func_line(input: &[u8]) -> IResult<&[u8], Function> {
    let (input, _) = terminated(tag("FUNC"), space1)(input)?;
    let (input, (_multiple, address, size, parameter_size, name)) = cut(tuple((
        opt(terminated(tag("m"), space1)),
        terminated(hex_str::<u64>, space1),
        terminated(hex_str::<u32>, space1),
        terminated(hex_str::<u32>, space1),
        terminated(map_res(not_my_eol, str::from_utf8), my_eol),
    )))(input)?;
    Ok((
        input,
        Function {
            address,
            size,
            parameter_size,
            name: name.to_string(),
            lines: RangeMap::new(),
            inlinees: Vec::new(),
        },
    ))
}

// Matches one entry of the form <address> <size> which is used at the end of an INLINE record
fn inline_address_range(input: &[u8]) -> IResult<&[u8], (u64, u32)> {
    tuple((terminated(hex_str::<u64>, space1), hex_str::<u32>))(input)
}

// Matches an INLINE record.
///
/// An INLINE record has the form `INLINE <inline_nest_level> <call_site_line> <call_site_file_id> <origin_id> [<address> <size>]+`.
fn inline_line(input: &[u8]) -> IResult<&[u8], impl Iterator<Item = Inlinee>> {
    let (input, _) = terminated(tag("INLINE"), space1)(input)?;
    let (input, (depth, call_line, call_file, origin_id)) = cut(tuple((
        terminated(decimal_u32, space1),
        terminated(decimal_u32, space1),
        terminated(decimal_u32, space1),
        terminated(decimal_u32, space1),
    )))(input)?;
    let (input, address_ranges) = cut(terminated(
        separated_list1(space1, inline_address_range),
        my_eol,
    ))(input)?;
    Ok((
        input,
        address_ranges
            .into_iter()
            .map(move |(address, size)| Inlinee {
                address,
                size,
                call_file,
                call_line,
                depth,
                origin_id,
            }),
    ))
}

// Matches a STACK WIN record.
fn stack_win_line(input: &[u8]) -> IResult<&[u8], WinFrameType> {
    let (input, _) = terminated(tag("STACK WIN"), space1)(input)?;
    let (
        input,
        (
            ty,
            address,
            code_size,
            prologue_size,
            epilogue_size,
            parameter_size,
            saved_register_size,
            local_size,
            max_stack_size,
            has_program_string,
            rest,
        ),
    ) = cut(tuple((
        terminated(single(is_hex_digit), space1), // ty
        terminated(hex_str::<u64>, space1),       // address
        terminated(hex_str::<u32>, space1),       // code_size
        terminated(hex_str::<u32>, space1),       // prologue_size
        terminated(hex_str::<u32>, space1),       // epilogue_size
        terminated(hex_str::<u32>, space1),       // parameter_size
        terminated(hex_str::<u32>, space1),       // saved_register_size
        terminated(hex_str::<u32>, space1),       // local_size
        terminated(hex_str::<u32>, space1),       // max_stack_size
        terminated(map(single(is_digit), |b| b == b'1'), space1), // has_program_string
        terminated(map_res(not_my_eol, str::from_utf8), my_eol),
    )))(input)?;

    // Sometimes has_program_string is just wrong. We could try to infer which one is right
    // but this is rare enough that it's better to just play it safe and discard the input.
    let really_has_program_string = ty == b'4';
    if really_has_program_string != has_program_string {
        let kind = match ty {
            b'4' => "FrameData",
            b'0' => "Fpo",
            _ => "Unknown Type!",
        };
        warn!("STACK WIN entry had inconsistent values for type and has_program_string, discarding corrupt entry");
        // warn!("  {}", &line);
        warn!(
            "  type: {} ({}), has_program_string: {}, final_arg: {}",
            str::from_utf8(&[ty]).unwrap_or(""),
            kind,
            has_program_string,
            rest
        );

        return Ok((input, WinFrameType::Unhandled));
    }

    let program_string_or_base_pointer = if really_has_program_string {
        WinStackThing::ProgramString(rest.to_string())
    } else {
        WinStackThing::AllocatesBasePointer(rest == "1")
    };
    let info = StackInfoWin {
        address,
        size: code_size,
        prologue_size,
        epilogue_size,
        parameter_size,
        saved_register_size,
        local_size,
        max_stack_size,
        program_string_or_base_pointer,
    };
    let frame_type = match ty {
        b'4' => WinFrameType::FrameData(info),
        b'0' => WinFrameType::Fpo(info),
        _ => WinFrameType::Unhandled,
    };
    Ok((input, frame_type))
}

// Matches a STACK CFI record.
fn stack_cfi(input: &[u8]) -> IResult<&[u8], CfiRules> {
    let (input, _) = terminated(tag("STACK CFI"), space1)(input)?;
    let (input, (address, rules)) = cut(tuple((
        terminated(hex_str::<u64>, space1),
        terminated(map_res(not_my_eol, str::from_utf8), my_eol),
    )))(input)?;
    Ok((
        input,
        CfiRules {
            address,
            rules: rules.to_string(),
        },
    ))
}

// Matches a STACK CFI INIT record.
fn stack_cfi_init(input: &[u8]) -> IResult<&[u8], StackInfoCfi> {
    let (input, _) = terminated(tag("STACK CFI INIT"), space1)(input)?;
    let (input, (address, size, rules)) = cut(tuple((
        terminated(hex_str::<u64>, space1),
        terminated(hex_str::<u32>, space1),
        terminated(map_res(not_my_eol, str::from_utf8), my_eol),
    )))(input)?;
    Ok((
        input,
        StackInfoCfi {
            init: CfiRules {
                address,
                rules: rules.to_string(),
            },
            size,
            add_rules: Default::default(),
        },
    ))
}

// Parse any of the line data that can occur in the body of a symbol file.
fn line(input: &[u8]) -> IResult<&[u8], Line> {
    alt((
        map(info_url, Line::Info),
        map(info_line, |_| Line::Info(Info::Unknown)),
        map(file_line, |(i, f)| Line::File(i, f)),
        map(inline_origin_line, |(i, f)| Line::InlineOrigin(i, f)),
        map(public_line, Line::Public),
        map(func_line, |f| Line::Function(f, Vec::new(), Vec::new())),
        map(stack_win_line, Line::StackWin),
        map(stack_cfi_init, Line::StackCfi),
        map(module_line, |(p, a, i, f)| Line::Module(p, a, i, f)),
    ))(input)
}

/// A parser for SymbolFiles.
///
/// This is basically just a SymbolFile but with some extra state
/// to handle streaming parsing.
///
/// Use this by repeatedly calling [`parse_more`][] until the
/// whole input is consumed. Then call [`finish`][].
#[derive(Debug, Default)]
pub struct SymbolParser {
    module_id: String,
    debug_file: String,
    files: HashMap<u32, String>,
    inline_origins: HashMap<u32, String>,
    publics: Vec<PublicSymbol>,

    // When building a RangeMap when need to sort an array of this
    // format anyway, so we might as well construct it directly and
    // save a giant allocation+copy.
    functions: Vec<(Range<u64>, Function)>,
    cfi_stack_info: Vec<(Range<u64>, StackInfoCfi)>,
    win_stack_framedata_info: Vec<(Range<u64>, StackInfoWin)>,
    win_stack_fpo_info: Vec<(Range<u64>, StackInfoWin)>,
    url: Option<String>,
    pub lines: u64,
    cur_item: Option<Line>,
}

impl SymbolParser {
    /// Creates a new SymbolParser.
    pub fn new() -> Self {
        Self::default()
    }

    /// Parses as much of the input as it can, and then returns
    /// how many bytes of the input was used. The *unused* portion of the
    /// input must be resubmitted on subsequent calls to parse_more
    /// (along with more data so we can make progress on the parse).
    pub fn parse_more(&mut self, mut input: &[u8]) -> Result<usize, SymbolError> {
        // We parse the input line-by-line, so trim away any part of the input
        // that comes after the last newline (this is necessary for streaming
        // parsing, as it can otherwise be impossible to tell if a line is
        // truncated.)
        input = if let Some(idx) = input.iter().rposition(|&x| x == b'\n') {
            &input[..idx + 1]
        } else {
            // If there's no newline, then we can't process anything!
            return Ok(0);
        };
        // Remember the (truncated) input so that we can tell how many bytes
        // we've consumed.
        let orig_input = input;

        loop {
            // If there's no more input, then we've consumed all of it
            // (except for the partial line we trimmed away).
            if input.is_empty() {
                return Ok(orig_input.len());
            }

            // First check if we're currently processing sublines of a
            // multi-line item like `FUNC` and `STACK CFI INIT`.
            // If we are, parse the next line as its subline format.
            //
            // If we encounter an error, this probably means we've
            // reached a new item (which ends this one). To handle this,
            // we can just finish off the current item and resubmit this
            // line to the top-level parser (below). If the line is
            // genuinely corrupt, then the top-level parser will also
            // fail to read it.
            //
            // We `take` and then reconstitute the item for borrowing/move
            // reasons.
            match self.cur_item.take() {
                Some(Line::Function(cur, mut lines, mut inlinees)) => {
                    match self.parse_func_subline(input, &mut lines, &mut inlinees) {
                        Ok((new_input, ())) => {
                            input = new_input;
                            self.cur_item = Some(Line::Function(cur, lines, inlinees));
                            self.lines += 1;
                            continue;
                        }
                        Err(_) => {
                            self.finish_item(Line::Function(cur, lines, inlinees));
                            continue;
                        }
                    }
                }
                Some(Line::StackCfi(mut cur)) => match stack_cfi(input) {
                    Ok((new_input, line)) => {
                        cur.add_rules.push(line);
                        input = new_input;
                        self.cur_item = Some(Line::StackCfi(cur));
                        self.lines += 1;
                        continue;
                    }
                    Err(_) => {
                        self.finish_item(Line::StackCfi(cur));
                        continue;
                    }
                },
                _ => {
                    // We're not parsing sublines, move on to top level parser!
                }
            }

            // Ignore empty lines
            if let Ok((new_input, _)) = my_eol(input) {
                input = new_input;
                self.lines += 1;
                continue;
            }

            // Parse a top-level item, and first handle the Result
            let line = match line(input) {
                Ok((new_input, line)) => {
                    // Success! Advance the input.
                    input = new_input;
                    line
                }
                Err(_) => {
                    // The file has a completely corrupt line,
                    // conservatively reject the entire parse.
                    return Err(SymbolError::ParseError("failed to parse file", self.lines));
                }
            };

            // Now store the item in our partial SymbolFile (or make it the cur_item
            // if it has potential sublines we need to parse first).
            match line {
                Line::Module(_platform, _arch, module_id, debug_file) => {
                    // We don't use this but it MUST be the first line
                    if self.lines != 0 {
                        return Err(SymbolError::ParseError(
                            "MODULE line found after the start of the file",
                            self.lines,
                        ));
                    }
                    self.module_id = module_id;
                    self.debug_file = debug_file;
                }
                Line::Info(Info::Url(cached_url)) => {
                    self.url = Some(cached_url);
                }
                Line::Info(Info::Unknown) => {
                    // Don't care
                }
                Line::File(id, filename) => {
                    self.files.insert(id, filename.to_string());
                }
                Line::InlineOrigin(id, function) => {
                    self.inline_origins.insert(id, function.to_string());
                }
                Line::Public(p) => {
                    self.publics.push(p);
                }
                Line::StackWin(frame_type) => {
                    // PDB files contain lots of overlapping unwind info, so we have to filter
                    // some of it out.
                    fn insert_win_stack_info(
                        stack_win: &mut Vec<(Range<u64>, StackInfoWin)>,
                        info: StackInfoWin,
                    ) {
                        if let Some(memory_range) = info.memory_range() {
                            if let Some((last_range, last_info)) = stack_win.last_mut() {
                                if last_range.intersects(&memory_range) {
                                    if info.address > last_info.address {
                                        // Sometimes we get STACK WIN directives where each line
                                        // has an accurate starting point, but the length just
                                        // covers the entire function, like so:
                                        //
                                        // addr: 0, len: 10
                                        // addr: 1, len: 9
                                        // addr: 4, len: 6
                                        //
                                        // In this case, the next instruction is the one that
                                        // really defines the length of the previous one. So
                                        // we need to fixup the lengths like so:
                                        //
                                        // addr: 0, len: 1
                                        // addr: 1, len: 2
                                        // addr: 4, len: 6
                                        last_info.size = (info.address - last_info.address) as u32;
                                        *last_range = last_info.memory_range().unwrap();
                                    } else if *last_range != memory_range {
                                        // We silently drop identical ranges because sometimes
                                        // duplicates happen, but we complain for non-trivial duplicates.
                                        warn!(
                                            "STACK WIN entry had bad intersections, dropping it {:?}",
                                            info
                                        );
                                        return;
                                    }
                                }
                            }
                            stack_win.push((memory_range, info));
                        } else {
                            warn!("STACK WIN entry had invalid range, dropping it {:?}", info);
                        }
                    }
                    match frame_type {
                        WinFrameType::FrameData(s) => {
                            insert_win_stack_info(&mut self.win_stack_framedata_info, s);
                        }
                        WinFrameType::Fpo(s) => {
                            insert_win_stack_info(&mut self.win_stack_fpo_info, s);
                        }
                        // Just ignore other types.
                        _ => {}
                    }
                }
                item @ Line::Function(_, _, _) => {
                    // More sublines to parse
                    self.cur_item = Some(item);
                }
                item @ Line::StackCfi(_) => {
                    // More sublines to parse
                    self.cur_item = Some(item);
                }
            }

            // Make note that we've consumed a line of input.
            self.lines += 1;
        }
    }

    /// Parses a single line which is following a FUNC line.
    fn parse_func_subline<'a>(
        &mut self,
        input: &'a [u8],
        lines: &mut Vec<SourceLine>,
        inlinees: &mut Vec<Inlinee>,
    ) -> IResult<&'a [u8], ()> {
        // We can have three different types of sublines: INLINE_ORIGIN, INLINE, or line records.
        // Check them one by one.
        // We're not using nom's `alt()` here because we'd need to find a common return type.
        if input.starts_with(b"INLINE_ORIGIN ") {
            let (input, (id, function)) = inline_origin_line(input)?;
            self.inline_origins.insert(id, function);
            return Ok((input, ()));
        }
        if input.starts_with(b"INLINE ") {
            let (input, new_inlinees) = inline_line(input)?;
            inlinees.extend(new_inlinees);
            return Ok((input, ()));
        }
        let (input, line) = func_line_data(input)?;
        lines.push(line);
        Ok((input, ()))
    }

    /// Finish processing an item (cur_item) which had sublines.
    /// We now have all the sublines, so it's complete.
    fn finish_item(&mut self, item: Line) {
        match item {
            Line::Function(mut cur, lines, mut inlinees) => {
                cur.lines = lines
                    .into_iter()
                    // Line data from PDB files often has a zero-size line entry, so just
                    // filter those out.
                    .filter(|l| l.size > 0)
                    .map(|l| {
                        let end_address = l.address.checked_add(l.size as u64 - 1);
                        let range = end_address.map(|end| Range::new(l.address, end));
                        (range, l)
                    })
                    .into_rangemap_safe();

                inlinees.sort();
                cur.inlinees = inlinees;

                if let Some(range) = cur.memory_range() {
                    self.functions.push((range, cur));
                }
            }
            Line::StackCfi(mut cur) => {
                cur.add_rules.sort();
                if let Some(range) = cur.memory_range() {
                    self.cfi_stack_info.push((range, cur));
                }
            }
            _ => {
                unreachable!()
            }
        }
    }

    /// Finish the parse and create the final SymbolFile.
    ///
    /// Call this when the parser has consumed all the input.
    pub fn finish(mut self) -> SymbolFile {
        // If there's a pending multiline item, finish it now.
        if let Some(item) = self.cur_item.take() {
            self.finish_item(item);
        }

        // Now sort everything and bundle it up in its final format.
        self.publics.sort();

        SymbolFile {
            module_id: self.module_id,
            debug_file: self.debug_file,
            files: self.files,
            publics: self.publics,
            functions: into_rangemap_safe(self.functions),
            inline_origins: self.inline_origins,
            cfi_stack_info: into_rangemap_safe(self.cfi_stack_info),
            win_stack_framedata_info: into_rangemap_safe(self.win_stack_framedata_info),
            win_stack_fpo_info: into_rangemap_safe(self.win_stack_fpo_info),
            // Will get filled in by the caller
            url: self.url,
            ambiguities_repaired: 0,
            ambiguities_discarded: 0,
            corruptions_discarded: 0,
            cfi_eval_corruptions: 0,
        }
    }
}

// Copied from minidump-common, because we've preconstructed the array to sort.
fn into_rangemap_safe<V: Clone + Eq + Debug>(mut input: Vec<(Range<u64>, V)>) -> RangeMap<u64, V> {
    input.sort_by_key(|x| x.0);
    let mut vec: Vec<(Range<u64>, V)> = Vec::with_capacity(input.len());
    for (range, val) in input {
        if let Some((last_range, last_val)) = vec.last_mut() {
            if range.start <= last_range.end && val != *last_val {
                continue;
            }

            if range.start <= last_range.end.saturating_add(1) && &val == last_val {
                last_range.end = std::cmp::max(range.end, last_range.end);
                continue;
            }
        }
        vec.push((range, val));
    }
    RangeMap::try_from_iter(vec).unwrap()
}

#[cfg(test)]
fn parse_symbol_bytes(data: &[u8]) -> Result<SymbolFile, SymbolError> {
    SymbolFile::parse(data, |_| ())
}

#[test]
fn test_module_line() {
    let line = b"MODULE Linux x86 D3096ED481217FD4C16B29CD9BC208BA0 firefox-bin\n";
    let rest = &b""[..];
    assert_eq!(
        module_line(line),
        Ok((
            rest,
            (
                "Linux".to_string(),
                "x86".to_string(),
                "D3096ED481217FD4C16B29CD9BC208BA0".to_string(),
                "firefox-bin".to_string()
            )
        ))
    );
}

#[test]
fn test_module_line_filename_spaces() {
    let line = b"MODULE Windows x86_64 D3096ED481217FD4C16B29CD9BC208BA0 firefox x y z\n";
    let rest = &b""[..];
    assert_eq!(
        module_line(line),
        Ok((
            rest,
            (
                "Windows".to_string(),
                "x86_64".to_string(),
                "D3096ED481217FD4C16B29CD9BC208BA0".to_string(),
                "firefox x y z".to_string()
            )
        ))
    );
}

/// Sometimes dump_syms on Windows does weird things and produces multiple carriage returns
/// before the line feed.
#[test]
fn test_module_line_crcrlf() {
    let line = b"MODULE Windows x86_64 D3096ED481217FD4C16B29CD9BC208BA0 firefox\r\r\n";
    let rest = &b""[..];
    assert_eq!(
        module_line(line),
        Ok((
            rest,
            (
                "Windows".to_string(),
                "x86_64".to_string(),
                "D3096ED481217FD4C16B29CD9BC208BA0".to_string(),
                "firefox".to_string()
            )
        ))
    );
}

#[test]
fn test_info_line() {
    let line = b"INFO blah blah blah\n";
    let bits = &b"blah blah blah"[..];
    let rest = &b""[..];
    assert_eq!(info_line(line), Ok((rest, bits)));
}

#[test]
fn test_info_line2() {
    let line = b"INFO   CODE_ID   abc xyz\n";
    let bits = &b"CODE_ID   abc xyz"[..];
    let rest = &b""[..];
    assert_eq!(info_line(line), Ok((rest, bits)));
}

#[test]
fn test_info_url() {
    let line = b"INFO URL https://www.example.com\n";
    let url = "https://www.example.com".to_string();
    let rest = &b""[..];
    assert_eq!(info_url(line), Ok((rest, Info::Url(url))));
}

#[test]
fn test_file_line() {
    let line = b"FILE 1 foo.c\n";
    let rest = &b""[..];
    assert_eq!(file_line(line), Ok((rest, (1, String::from("foo.c")))));
}

#[test]
fn test_file_line_spaces() {
    let line = b"FILE  1234  foo bar.xyz\n";
    let rest = &b""[..];
    assert_eq!(
        file_line(line),
        Ok((rest, (1234, String::from("foo bar.xyz"))))
    );
}

#[test]
fn test_public_line() {
    let line = b"PUBLIC f00d d00d some func\n";
    let rest = &b""[..];
    assert_eq!(
        public_line(line),
        Ok((
            rest,
            PublicSymbol {
                address: 0xf00d,
                parameter_size: 0xd00d,
                name: "some func".to_string(),
            }
        ))
    );
}

#[test]
fn test_public_with_m() {
    let line = b"PUBLIC m f00d d00d some func\n";
    let rest = &b""[..];
    assert_eq!(
        public_line(line),
        Ok((
            rest,
            PublicSymbol {
                address: 0xf00d,
                parameter_size: 0xd00d,
                name: "some func".to_string(),
            }
        ))
    );
}

#[test]
fn test_func_lines_no_lines() {
    use range_map::RangeMap;
    let line = b"FUNC c184 30 0 nsQueryInterfaceWithError::operator()(nsID const&, void**) const\n";
    let rest = &b""[..];
    assert_eq!(
        func_line(line),
        Ok((
            rest,
            Function {
                address: 0xc184,
                size: 0x30,
                parameter_size: 0,
                name: "nsQueryInterfaceWithError::operator()(nsID const&, void**) const"
                    .to_string(),
                lines: RangeMap::new(),
                inlinees: Vec::new(),
            }
        ))
    );
}

#[test]
fn test_truncated_func() {
    let line = b"FUNC 1000\n1000 10 42 7\n";
    assert_eq!(
        func_line(line),
        Err(Err::Failure(Error {
            input: &line[9..],
            code: ErrorKind::Space
        }))
    );
}

#[test]
fn test_inline_line_single_range() {
    let line = b"INLINE 0 3082 52 1410 49200 10\n";
    assert_eq!(
        inline_line(line).unwrap().1.collect::<Vec<_>>(),
        vec![Inlinee {
            depth: 0,
            address: 0x49200,
            size: 0x10,
            call_file: 52,
            call_line: 3082,
            origin_id: 1410
        }]
    )
}

#[test]
fn test_inline_line_multiple_ranges() {
    let line = b"INLINE 6 642 8 207 8b110 18 8b154 18\n";
    assert_eq!(
        inline_line(line).unwrap().1.collect::<Vec<_>>(),
        vec![
            Inlinee {
                depth: 6,
                address: 0x8b110,
                size: 0x18,
                call_file: 8,
                call_line: 642,
                origin_id: 207
            },
            Inlinee {
                depth: 6,
                address: 0x8b154,
                size: 0x18,
                call_file: 8,
                call_line: 642,
                origin_id: 207
            }
        ]
    )
}

#[test]
fn test_func_lines_and_lines() {
    let data = b"FUNC 1000 30 10 some func
1000 10 42 7
INLINE_ORIGIN 16 inlined_function_name()
1010 10 52 8
INLINE 0 23 9 16 1020 10
1020 10 62 15
";
    let file = SymbolFile::from_bytes(data).expect("failed to parse!");
    let (_, f) = file.functions.ranges_values().next().unwrap();
    assert_eq!(f.address, 0x1000);
    assert_eq!(f.size, 0x30);
    assert_eq!(f.parameter_size, 0x10);
    assert_eq!(f.name, "some func".to_string());
    assert_eq!(
        f.lines.get(0x1000).unwrap(),
        &SourceLine {
            address: 0x1000,
            size: 0x10,
            file: 7,
            line: 42,
        }
    );
    assert_eq!(
        f.lines.ranges_values().collect::<Vec<_>>(),
        vec![
            &(
                Range::<u64>::new(0x1000, 0x100F),
                SourceLine {
                    address: 0x1000,
                    size: 0x10,
                    file: 7,
                    line: 42,
                },
            ),
            &(
                Range::<u64>::new(0x1010, 0x101F),
                SourceLine {
                    address: 0x1010,
                    size: 0x10,
                    file: 8,
                    line: 52,
                },
            ),
            &(
                Range::<u64>::new(0x1020, 0x102F),
                SourceLine {
                    address: 0x1020,
                    size: 0x10,
                    file: 15,
                    line: 62,
                },
            ),
        ]
    );
    assert_eq!(
        f.inlinees,
        vec![Inlinee {
            depth: 0,
            address: 0x1020,
            size: 0x10,
            call_file: 9,
            call_line: 23,
            origin_id: 16
        }]
    );
}

#[test]
fn test_nested_inlines() {
    // 0x1000: outer_func() @ <file 15>:60 -> mid_func() @ <file 4>:12 -> inner_func1() <file 7>:42
    // 0x1010: outer_func() @ <file 15>:60 -> mid_func() @ <file 4>:17 -> inner_func2() <file 8>:52
    // 0x1020: outer_func() @ <file 15>:62
    let data = b"FUNC 1000 30 10 outer_func()
INLINE_ORIGIN 1 inner_func_2()
INLINE_ORIGIN 2 mid_func()
INLINE_ORIGIN 3 inner_func_1()
INLINE 0 60 15 2 1000 20
INLINE 1 12 4 3 1000 10
INLINE 1 17 4 1 1010 10
1000 10 42 7
1010 10 52 8
1020 10 62 15
";
    let file = SymbolFile::from_bytes(data).expect("failed to parse!");
    let (_, f) = file.functions.ranges_values().next().unwrap();
    assert_eq!(f.address, 0x1000);
    assert_eq!(f.size, 0x30);
    assert_eq!(f.parameter_size, 0x10);
    assert_eq!(f.name, "outer_func()".to_string());

    // Check the source locations at the "outermost" level, i.e. the line
    // numbers inside the "outer_func()" function. This function has its
    // code in file 15, so all source locations at this level should be
    // in that file.
    assert_eq!(f.get_outermost_sourceloc(0x0fff), None);
    assert_eq!(
        f.get_outermost_sourceloc(0x1000),
        Some((15, 60, 0x1000, Some(2)))
    );
    assert_eq!(
        f.get_outermost_sourceloc(0x100f),
        Some((15, 60, 0x1000, Some(2)))
    );
    assert_eq!(
        f.get_outermost_sourceloc(0x1010),
        Some((15, 60, 0x1000, Some(2)))
    );
    assert_eq!(
        f.get_outermost_sourceloc(0x101f),
        Some((15, 60, 0x1000, Some(2)))
    );
    assert_eq!(
        f.get_outermost_sourceloc(0x1020),
        Some((15, 62, 0x1020, None))
    );
    assert_eq!(
        f.get_outermost_sourceloc(0x102f),
        Some((15, 62, 0x1020, None))
    );
    assert_eq!(f.get_outermost_sourceloc(0x1030), None);

    // Check the first level of inlining. There is only one inlined call
    // at this level, the call from outer_func() to mid_func(), spanning
    // the range 0x1000..0x1020.
    assert_eq!(f.get_inlinee_at_depth(0, 0x0fff), None);
    assert_eq!(f.get_inlinee_at_depth(0, 0x1000), Some((15, 60, 0x1000, 2)));
    assert_eq!(f.get_inlinee_at_depth(0, 0x100f), Some((15, 60, 0x1000, 2)));
    assert_eq!(f.get_inlinee_at_depth(0, 0x1010), Some((15, 60, 0x1000, 2)));
    assert_eq!(f.get_inlinee_at_depth(0, 0x101f), Some((15, 60, 0x1000, 2)));
    assert_eq!(f.get_inlinee_at_depth(0, 0x1020), None);
    assert_eq!(f.get_inlinee_at_depth(0, 0x102f), None);
    assert_eq!(f.get_inlinee_at_depth(0, 0x1030), None);

    // Check the second level of inlining. Two function calls from mid_func()
    // have been inlined at this level, the call to inner_func_1() and the
    // call to inner_func_2().
    // The code for mid_func() is in file 4, so the location of the calls to
    // inner_func_1() and inner_func_2() are in file 4.
    assert_eq!(f.get_inlinee_at_depth(1, 0x0fff), None);
    assert_eq!(f.get_inlinee_at_depth(1, 0x1000), Some((4, 12, 0x1000, 3)));
    assert_eq!(f.get_inlinee_at_depth(1, 0x100f), Some((4, 12, 0x1000, 3)));
    assert_eq!(f.get_inlinee_at_depth(1, 0x1010), Some((4, 17, 0x1010, 1)));
    assert_eq!(f.get_inlinee_at_depth(1, 0x101f), Some((4, 17, 0x1010, 1)));
    assert_eq!(f.get_inlinee_at_depth(1, 0x1020), None);
    assert_eq!(f.get_inlinee_at_depth(1, 0x102f), None);
    assert_eq!(f.get_inlinee_at_depth(1, 0x1030), None);

    // Check that there are no deeper inline calls.
    assert_eq!(f.get_inlinee_at_depth(2, 0x0fff), None);
    assert_eq!(f.get_inlinee_at_depth(2, 0x1000), None);
    assert_eq!(f.get_inlinee_at_depth(2, 0x100f), None);
    assert_eq!(f.get_inlinee_at_depth(2, 0x1010), None);
    assert_eq!(f.get_inlinee_at_depth(2, 0x101f), None);
    assert_eq!(f.get_inlinee_at_depth(2, 0x1020), None);
    assert_eq!(f.get_inlinee_at_depth(2, 0x102f), None);
    assert_eq!(f.get_inlinee_at_depth(2, 0x1030), None);

    // Check the "innermost" source locations. These locations describe the
    // file and line at the deepest level of inlining at the given address.
    // We have a location in inner_func_1() (whose code is in file 7), a location
    // in inner_func_2() (whose code is in file 8), and a location in the outer
    // function outer_func() (whose code is in file 15).
    assert_eq!(f.get_innermost_sourceloc(0x0fff), None);
    assert_eq!(f.get_innermost_sourceloc(0x1000), Some((7, 42, 0x1000)));
    assert_eq!(f.get_innermost_sourceloc(0x100f), Some((7, 42, 0x1000)));
    assert_eq!(f.get_innermost_sourceloc(0x1010), Some((8, 52, 0x1010)));
    assert_eq!(f.get_innermost_sourceloc(0x101f), Some((8, 52, 0x1010)));
    assert_eq!(f.get_innermost_sourceloc(0x1020), Some((15, 62, 0x1020)));
    assert_eq!(f.get_innermost_sourceloc(0x102f), Some((15, 62, 0x1020)));
    assert_eq!(f.get_innermost_sourceloc(0x1030), None);
}

#[test]
fn test_func_with_m() {
    let data = b"FUNC m 1000 30 10 some func
1000 10 42 7
1010 10 52 8
1020 10 62 15
";
    let file = SymbolFile::from_bytes(data).expect("failed to parse!");
    let (_, _f) = file.functions.ranges_values().next().unwrap();
}

#[test]
fn test_stack_win_line_program_string() {
    let line =
        b"STACK WIN 4 2170 14 a1 b2 c3 d4 e5 f6 1 $eip 4 + ^ = $esp $ebp 8 + = $ebp $ebp ^ =\n";
    match stack_win_line(line) {
        Ok((rest, WinFrameType::FrameData(stack))) => {
            assert_eq!(rest, &b""[..]);
            assert_eq!(stack.address, 0x2170);
            assert_eq!(stack.size, 0x14);
            assert_eq!(stack.prologue_size, 0xa1);
            assert_eq!(stack.epilogue_size, 0xb2);
            assert_eq!(stack.parameter_size, 0xc3);
            assert_eq!(stack.saved_register_size, 0xd4);
            assert_eq!(stack.local_size, 0xe5);
            assert_eq!(stack.max_stack_size, 0xf6);
            assert_eq!(
                stack.program_string_or_base_pointer,
                WinStackThing::ProgramString(
                    "$eip 4 + ^ = $esp $ebp 8 + = $ebp $ebp ^ =".to_string()
                )
            );
        }
        Err(e) => panic!("{}", format!("Parse error: {e:?}")),
        _ => panic!("Something bad happened"),
    }
}

#[test]
fn test_stack_win_line_frame_data() {
    let line = b"STACK WIN 0 1000 30 a1 b2 c3 d4 e5 f6 0 1\n";
    match stack_win_line(line) {
        Ok((rest, WinFrameType::Fpo(stack))) => {
            assert_eq!(rest, &b""[..]);
            assert_eq!(stack.address, 0x1000);
            assert_eq!(stack.size, 0x30);
            assert_eq!(stack.prologue_size, 0xa1);
            assert_eq!(stack.epilogue_size, 0xb2);
            assert_eq!(stack.parameter_size, 0xc3);
            assert_eq!(stack.saved_register_size, 0xd4);
            assert_eq!(stack.local_size, 0xe5);
            assert_eq!(stack.max_stack_size, 0xf6);
            assert_eq!(
                stack.program_string_or_base_pointer,
                WinStackThing::AllocatesBasePointer(true)
            );
        }
        Err(e) => panic!("{}", format!("Parse error: {e:?}")),
        _ => panic!("Something bad happened"),
    }
}

#[test]
fn test_stack_cfi() {
    let line = b"STACK CFI deadf00d some rules\n";
    let rest = &b""[..];
    assert_eq!(
        stack_cfi(line),
        Ok((
            rest,
            CfiRules {
                address: 0xdeadf00d,
                rules: "some rules".to_string(),
            }
        ))
    );
}

#[test]
fn test_stack_cfi_init() {
    let line = b"STACK CFI INIT badf00d abc init rules\n";
    let rest = &b""[..];
    assert_eq!(
        stack_cfi_init(line),
        Ok((
            rest,
            StackInfoCfi {
                init: CfiRules {
                    address: 0xbadf00d,
                    rules: "init rules".to_string(),
                },
                size: 0xabc,
                add_rules: vec![],
            }
        ))
    );
}

#[test]
fn test_stack_cfi_lines() {
    let data = b"STACK CFI INIT badf00d abc init rules
STACK CFI deadf00d some rules
STACK CFI deadbeef more rules

";
    let file = SymbolFile::from_bytes(data).expect("failed to parse!");
    let (_, cfi) = file.cfi_stack_info.ranges_values().next().unwrap();
    assert_eq!(
        cfi,
        &StackInfoCfi {
            init: CfiRules {
                address: 0xbadf00d,
                rules: "init rules".to_string(),
            },
            size: 0xabc,
            add_rules: vec![
                CfiRules {
                    address: 0xdeadbeef,
                    rules: "more rules".to_string(),
                },
                CfiRules {
                    address: 0xdeadf00d,
                    rules: "some rules".to_string(),
                },
            ],
        }
    );
}

#[test]
fn test_parse_symbol_bytes() {
    let bytes = &b"MODULE Linux x86 D3096ED481217FD4C16B29CD9BC208BA0 firefox-bin
INFO blah blah blah
FILE 0 foo.c
FILE 100 bar.c
PUBLIC abcd 10 func 1
PUBLIC ff00 3 func 2
FUNC 900 30 10 some other func
FUNC 1000 30 10 some func
1000 10 42 7
1010 10 52 8
1020 10 62 15
FUNC 1100 30 10 a third func
STACK WIN 4 900 30 a1 b2 c3 d4 e5 f6 1 prog string
STACK WIN 0 1000 30 a1 b2 c3 d4 e5 f6 0 1
STACK CFI INIT badf00d abc init rules
STACK CFI deadf00d some rules
STACK CFI deadbeef more rules
STACK CFI INIT f00f f0 more init rules

"[..];
    let sym = parse_symbol_bytes(bytes).unwrap();
    assert_eq!(sym.files.len(), 2);
    assert_eq!(sym.files.get(&0).unwrap(), "foo.c");
    assert_eq!(sym.files.get(&100).unwrap(), "bar.c");
    assert_eq!(sym.publics.len(), 2);
    {
        let p = &sym.publics[0];
        assert_eq!(p.address, 0xabcd);
        assert_eq!(p.parameter_size, 0x10);
        assert_eq!(p.name, "func 1".to_string());
    }
    {
        let p = &sym.publics[1];
        assert_eq!(p.address, 0xff00);
        assert_eq!(p.parameter_size, 0x3);
        assert_eq!(p.name, "func 2".to_string());
    }
    assert_eq!(sym.functions.ranges_values().count(), 3);
    let funcs = sym
        .functions
        .ranges_values()
        .map(|(_, f)| f)
        .collect::<Vec<_>>();
    {
        let f = &funcs[0];
        assert_eq!(f.address, 0x900);
        assert_eq!(f.size, 0x30);
        assert_eq!(f.parameter_size, 0x10);
        assert_eq!(f.name, "some other func".to_string());
        assert_eq!(f.lines.ranges_values().count(), 0);
    }
    {
        let f = &funcs[1];
        assert_eq!(f.address, 0x1000);
        assert_eq!(f.size, 0x30);
        assert_eq!(f.parameter_size, 0x10);
        assert_eq!(f.name, "some func".to_string());
        assert_eq!(
            f.lines.ranges_values().collect::<Vec<_>>(),
            vec![
                &(
                    Range::new(0x1000, 0x100F),
                    SourceLine {
                        address: 0x1000,
                        size: 0x10,
                        file: 7,
                        line: 42,
                    },
                ),
                &(
                    Range::new(0x1010, 0x101F),
                    SourceLine {
                        address: 0x1010,
                        size: 0x10,
                        file: 8,
                        line: 52,
                    },
                ),
                &(
                    Range::new(0x1020, 0x102F),
                    SourceLine {
                        address: 0x1020,
                        size: 0x10,
                        file: 15,
                        line: 62,
                    },
                ),
            ]
        );
    }
    {
        let f = &funcs[2];
        assert_eq!(f.address, 0x1100);
        assert_eq!(f.size, 0x30);
        assert_eq!(f.parameter_size, 0x10);
        assert_eq!(f.name, "a third func".to_string());
        assert_eq!(f.lines.ranges_values().count(), 0);
    }
    assert_eq!(sym.win_stack_framedata_info.ranges_values().count(), 1);
    let ws = sym
        .win_stack_framedata_info
        .ranges_values()
        .map(|(_, s)| s)
        .collect::<Vec<_>>();
    {
        let stack = &ws[0];
        assert_eq!(stack.address, 0x900);
        assert_eq!(stack.size, 0x30);
        assert_eq!(stack.prologue_size, 0xa1);
        assert_eq!(stack.epilogue_size, 0xb2);
        assert_eq!(stack.parameter_size, 0xc3);
        assert_eq!(stack.saved_register_size, 0xd4);
        assert_eq!(stack.local_size, 0xe5);
        assert_eq!(stack.max_stack_size, 0xf6);
        assert_eq!(
            stack.program_string_or_base_pointer,
            WinStackThing::ProgramString("prog string".to_string())
        );
    }
    assert_eq!(sym.win_stack_fpo_info.ranges_values().count(), 1);
    let ws = sym
        .win_stack_fpo_info
        .ranges_values()
        .map(|(_, s)| s)
        .collect::<Vec<_>>();
    {
        let stack = &ws[0];
        assert_eq!(stack.address, 0x1000);
        assert_eq!(stack.size, 0x30);
        assert_eq!(stack.prologue_size, 0xa1);
        assert_eq!(stack.epilogue_size, 0xb2);
        assert_eq!(stack.parameter_size, 0xc3);
        assert_eq!(stack.saved_register_size, 0xd4);
        assert_eq!(stack.local_size, 0xe5);
        assert_eq!(stack.max_stack_size, 0xf6);
        assert_eq!(
            stack.program_string_or_base_pointer,
            WinStackThing::AllocatesBasePointer(true)
        );
    }
    assert_eq!(sym.cfi_stack_info.ranges_values().count(), 2);
    let cs = sym
        .cfi_stack_info
        .ranges_values()
        .map(|(_, s)| s.clone())
        .collect::<Vec<_>>();
    assert_eq!(
        cs[0],
        StackInfoCfi {
            init: CfiRules {
                address: 0xf00f,
                rules: "more init rules".to_string(),
            },
            size: 0xf0,
            add_rules: vec![],
        }
    );
    assert_eq!(
        cs[1],
        StackInfoCfi {
            init: CfiRules {
                address: 0xbadf00d,
                rules: "init rules".to_string(),
            },
            size: 0xabc,
            add_rules: vec![
                CfiRules {
                    address: 0xdeadbeef,
                    rules: "more rules".to_string(),
                },
                CfiRules {
                    address: 0xdeadf00d,
                    rules: "some rules".to_string(),
                },
            ],
        }
    );
}

/// Test that parsing a symbol file with overlapping FUNC/line data works.
#[test]
fn test_parse_with_overlap() {
    //TODO: deal with duplicate PUBLIC records? Not as important since they don't go
    // into a RangeMap.
    let bytes = b"MODULE Linux x86 D3096ED481217FD4C16B29CD9BC208BA0 firefox-bin
FILE 0 foo.c
PUBLIC abcd 10 func 1
PUBLIC ff00 3 func 2
FUNC 1000 30 10 some func
1000 10 42 0
1000 10 43 0
1001 10 44 0
1001 5 45 0
1010 10 52 0
FUNC 1000 30 10 some func overlap exact
FUNC 1001 30 10 some func overlap end
FUNC 1001 10 10 some func overlap contained
";
    let sym = parse_symbol_bytes(&bytes[..]).unwrap();
    assert_eq!(sym.publics.len(), 2);
    {
        let p = &sym.publics[0];
        assert_eq!(p.address, 0xabcd);
        assert_eq!(p.parameter_size, 0x10);
        assert_eq!(p.name, "func 1".to_string());
    }
    {
        let p = &sym.publics[1];
        assert_eq!(p.address, 0xff00);
        assert_eq!(p.parameter_size, 0x3);
        assert_eq!(p.name, "func 2".to_string());
    }
    assert_eq!(sym.functions.ranges_values().count(), 1);
    let funcs = sym
        .functions
        .ranges_values()
        .map(|(_, f)| f)
        .collect::<Vec<_>>();
    {
        let f = &funcs[0];
        assert_eq!(f.address, 0x1000);
        assert_eq!(f.size, 0x30);
        assert_eq!(f.parameter_size, 0x10);
        assert_eq!(f.name, "some func".to_string());
        assert_eq!(
            f.lines.ranges_values().collect::<Vec<_>>(),
            vec![
                &(
                    Range::new(0x1000, 0x100F),
                    SourceLine {
                        address: 0x1000,
                        size: 0x10,
                        file: 0,
                        line: 42,
                    },
                ),
                &(
                    Range::new(0x1010, 0x101F),
                    SourceLine {
                        address: 0x1010,
                        size: 0x10,
                        file: 0,
                        line: 52,
                    },
                ),
            ]
        );
    }
}

#[test]
fn test_parse_symbol_bytes_malformed() {
    assert!(
        parse_symbol_bytes(&b"this is not a symbol file\n"[..]).is_err(),
        "Should fail to parse junk"
    );

    assert!(
        parse_symbol_bytes(
            &b"MODULE Linux x86 xxxxxx
FILE 0 foo.c
"[..]
        )
        .is_err(),
        "Should fail to parse malformed MODULE line"
    );

    assert!(
        parse_symbol_bytes(
            &b"MODULE Linux x86 abcd1234 foo
FILE x foo.c
"[..]
        )
        .is_err(),
        "Should fail to parse malformed FILE line"
    );

    assert!(
        parse_symbol_bytes(
            &b"MODULE Linux x86 abcd1234 foo
FUNC xx 1 2 foo
"[..]
        )
        .is_err(),
        "Should fail to parse malformed FUNC line"
    );

    assert!(
        parse_symbol_bytes(
            &b"MODULE Linux x86 abcd1234 foo
this is some junk
"[..]
        )
        .is_err(),
        "Should fail to parse malformed file"
    );

    assert!(
        parse_symbol_bytes(
            &b"MODULE Linux x86 abcd1234 foo
FILE 0 foo.c
FILE"[..]
        )
        .is_err(),
        "Should fail to parse truncated file"
    );

    assert!(
        parse_symbol_bytes(&b""[..]).is_err(),
        "Should fail to parse empty file"
    );
}

#[test]
fn test_parse_stack_win_inconsistent() {
    // Various cases where the has_program_string value is inconsistent
    // with the type of the STACK WIN entry.
    //
    // type=0 (FPO) should go with has_program_string==0 (false)
    // type=4 (FrameData) should go with has_program_string==1 (true)
    //
    // Only 4d93e and 8d93e are totally valid.
    //
    // Current policy is to discard all the other ones, but all the cases
    // are here in case we decide on a more sophisticated heuristic.

    let bytes = b"MODULE Windows x86 D3096ED481217FD4C16B29CD9BC208BA0 firefox-bin
FILE 0 foo.c
STACK WIN 0 1d93e 4 4 0 0 10 0 0 1 1
STACK WIN 0 2d93e 4 4 0 0 10 0 0 1 0
STACK WIN 0 3d93e 4 4 0 0 10 0 0 1 prog string
STACK WIN 0 4d93e 4 4 0 0 10 0 0 0 1
STACK WIN 4 5d93e 4 4 0 0 10 0 0 0 1
STACK WIN 4 6d93e 4 4 0 0 10 0 0 0 0
STACK WIN 4 7d93e 4 4 0 0 10 0 0 0 prog string
STACK WIN 4 8d93e 4 4 0 0 10 0 0 1 prog string
";
    let sym = parse_symbol_bytes(&bytes[..]).unwrap();

    assert_eq!(sym.win_stack_framedata_info.ranges_values().count(), 1);
    let ws = sym
        .win_stack_framedata_info
        .ranges_values()
        .map(|(_, s)| s)
        .collect::<Vec<_>>();
    {
        let stack = &ws[0];
        assert_eq!(stack.address, 0x8d93e);
        assert_eq!(stack.size, 0x4);
        assert_eq!(stack.prologue_size, 0x4);
        assert_eq!(stack.epilogue_size, 0);
        assert_eq!(stack.parameter_size, 0);
        assert_eq!(stack.saved_register_size, 0x10);
        assert_eq!(stack.local_size, 0);
        assert_eq!(stack.max_stack_size, 0);
        assert_eq!(
            stack.program_string_or_base_pointer,
            WinStackThing::ProgramString("prog string".to_string())
        );
    }
    assert_eq!(sym.win_stack_fpo_info.ranges_values().count(), 1);
    let ws = sym
        .win_stack_fpo_info
        .ranges_values()
        .map(|(_, s)| s)
        .collect::<Vec<_>>();
    {
        let stack = &ws[0];
        assert_eq!(stack.address, 0x4d93e);
        assert_eq!(stack.size, 0x4);
        assert_eq!(stack.prologue_size, 0x4);
        assert_eq!(stack.epilogue_size, 0);
        assert_eq!(stack.parameter_size, 0);
        assert_eq!(stack.saved_register_size, 0x10);
        assert_eq!(stack.local_size, 0);
        assert_eq!(stack.max_stack_size, 0);
        assert_eq!(
            stack.program_string_or_base_pointer,
            WinStackThing::AllocatesBasePointer(true)
        );
    }
}

#[test]
fn address_size_overflow() {
    let bytes = b"FUNC 1 2 3 x\nffffffffffffffff 2 0 0\n";
    let sym = parse_symbol_bytes(bytes.as_slice()).unwrap();
    let fun = sym.functions.get(1).unwrap();
    assert!(fun.lines.is_empty());
    assert!(fun.name == "x");
}
