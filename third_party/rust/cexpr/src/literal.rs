// (C) Copyright 2016 Jethro G. Beekman
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//! Parsing C literals from byte slices.
//! 
//! This will parse a representation of a C literal into a Rust type.
//!
//! # characters
//! Character literals are stored into the `CChar` type, which can hold values
//! that are not valid Unicode code points. ASCII characters are represented as
//! `char`, literal bytes with the high byte set are converted into the raw
//! representation. Escape sequences are supported. If hex and octal escapes
//! map to an ASCII character, that is used, otherwise, the raw encoding is
//! used, including for values over 255. Unicode escapes are checked for
//! validity and mapped to `char`. Character sequences are not supported. Width
//! prefixes are ignored.
//!
//! # strings
//! Strings are interpreted as byte vectors. Escape sequences are supported. If
//! hex and octal escapes map onto multi-byte characters, they are truncated to
//! one 8-bit character. Unicode escapes are converted into their UTF-8
//! encoding. Width prefixes are ignored.
//!
//! # integers
//! Integers are read into `i64`. Binary, octal, decimal and hexadecimal are
//! all supported. If the literal value is between `i64::MAX` and `u64::MAX`,
//! it is bit-cast to `i64`. Values over `u64::MAX` cannot be parsed. Width and
//! sign suffixes are ignored. Sign prefixes are not supported.
//!
//! # real numbers
//! Reals are read into `f64`. Width suffixes are ignored. Sign prefixes are
//! not supported in the significand. Hexadecimal floating points are not
//! supported.

use std::char;
use std::str::{self,FromStr};

use nom_crate::*;

use expr::EvalResult;

#[derive(Debug,Copy,Clone,PartialEq,Eq)]
/// Representation of a C character
pub enum CChar {
	/// A character that can be represented as a `char`
	Char(char),
	/// Any other character (8-bit characters, unicode surrogates, etc.)
	Raw(u64),
}

impl From<u8> for CChar {
	fn from(i: u8) -> CChar {
		match i {
			0 ... 0x7f => CChar::Char(i as u8 as char),
			_ => CChar::Raw(i as u64),
		}
	}
}

// A non-allocating version of this would be nice...
impl Into<Vec<u8>> for CChar {
	fn into(self) -> Vec<u8> {
		match self {
			CChar::Char(c) => {
				let mut s=String::with_capacity(4);
				s.extend(&[c]);
				s.into_bytes()
			}
			CChar::Raw(i) => {
				let mut v=Vec::with_capacity(1);
				v.push(i as u8);
				v
			}
		}
	}
}

/// ensures the child parser consumes the whole input
#[macro_export]
macro_rules! full (
	($i: expr, $submac:ident!( $($args:tt)* )) => (
		{
			use ::nom_crate::lib::std::result::Result::*;
			let res =  $submac!($i, $($args)*);
			match res {
				Ok((i, o)) => if i.len() == 0 {
					Ok((i, o))
				} else {
					Err(::nom_crate::Err::Error(error_position!(i, ::nom_crate::ErrorKind::Custom(42))))
				},
				r => r,
			}
		}
	);
	($i:expr, $f:ident) => (
		full!($i, call!($f));
	);
);

// ====================================================
// ======== macros that shouldn't be necessary ========
// ====================================================

macro_rules! force_type (
	($input:expr,IResult<$i:ty,$o:ty,$e:ty>) => (Err::<($i,$o),Err<$i,$e>>(::nom_crate::Err::Error(error_position!($input, ErrorKind::Fix))))
);


// =================================
// ======== matching digits ========
// =================================

macro_rules! byte (
	($i:expr, $($p: pat)|* ) => ({
		match $i.split_first() {
			$(Some((&c @ $p,rest)))|* => Ok::<(&[_],u8),::nom_crate::Err<&[_],u32>>((rest,c)),
			Some(_) => Err(::nom_crate::Err::Error(error_position!($i, ErrorKind::OneOf))),
			None => Err(::nom_crate::Err::Incomplete(Needed::Size(1))),
		}
	})
);

named!(binary<u8>,byte!(b'0' ... b'1'));
named!(octal<u8>,byte!(b'0' ... b'7'));
named!(decimal<u8>,byte!(b'0' ... b'9'));
named!(hexadecimal<u8>,byte!(b'0' ... b'9' | b'a' ... b'f' | b'A' ... b'F'));


// ========================================
// ======== characters and strings ========
// ========================================

fn escape2char(c: char) -> CChar {
	CChar::Char(match c {
		'a' => '\x07',
		'b' => '\x08',
		'f' => '\x0c',
		'n' => '\n',
		'r' => '\r',
		't' => '\t',
		'v' => '\x0b',
		_ => unreachable!("invalid escape {}",c)
	})
}

fn c_raw_escape(n: Vec<u8>, radix: u32) -> Option<CChar> {
	str::from_utf8(&n).ok()
		.and_then(|i|u64::from_str_radix(i,radix).ok())
		.map(|i|match i {
			0 ... 0x7f => CChar::Char(i as u8 as char),
			_ => CChar::Raw(i),
		})
}

fn c_unicode_escape(n: Vec<u8>) -> Option<CChar> {
	str::from_utf8(&n).ok()
		.and_then(|i|u32::from_str_radix(i,16).ok())
		.and_then(char::from_u32)
		.map(CChar::Char)
}

named!(escaped_char<CChar>,
	preceded!(complete!(char!('\\')),alt_complete!(
		map!(one_of!(r#"'"?\"#),CChar::Char) |
		map!(one_of!("abfnrtv"),escape2char) |
		map_opt!(many_m_n!(1,3,octal),|v|c_raw_escape(v,8)) |
		map_opt!(preceded!(char!('x'),many1!(hexadecimal)),|v|c_raw_escape(v,16)) |
		map_opt!(preceded!(char!('u'),many_m_n!(4,4,hexadecimal)),c_unicode_escape) |
		map_opt!(preceded!(char!('U'),many_m_n!(8,8,hexadecimal)),c_unicode_escape)
	))
);

named!(c_width_prefix,
	alt!(
		tag!("u8") |
		tag!("u") |
		tag!("U") |
		tag!("L")
	)
);

named!(c_char<CChar>,
	delimited!(
		terminated!(opt!(c_width_prefix),char!('\'')),
		alt!( escaped_char | map!(byte!(0 ... 91 /* \=92 */ | 93 ... 255),CChar::from) ),
		char!('\'')
	)
);

named!(c_string<Vec<u8> >,
	delimited!(
		alt!( preceded!(c_width_prefix,char!('"')) | char!('"') ),
		fold_many0!(
			alt!(map!(escaped_char, |c:CChar| c.into()) | map!(complete!(is_not!("\"")), |c: &[u8]| c.into())),
			Vec::new(),
			|mut v: Vec<u8>, res:Vec<u8>| { v.extend_from_slice(&res); v }
		),
		char!('"')
	)
);

// ================================
// ======== parse integers ========
// ================================

fn c_int_radix(n: Vec<u8>, radix: u32) -> Option<u64> {
	str::from_utf8(&n).ok()
		.and_then(|i|u64::from_str_radix(i,radix).ok())
}

fn take_ul(input: &[u8]) -> IResult<&[u8], &[u8]> {
	use ::nom_crate::InputTakeAtPosition;

	let r = input.split_at_position(|c| c != b'u' && c != b'U' && c != b'l' && c != b'L');
	match r {
		Err(Err::Incomplete(_)) => Ok((&input[input.len()..], input)),
		res => res,
	}
}

named!(c_int<i64>,
	map!(terminated!(alt_complete!(
		map_opt!(preceded!(tag!("0x"),many1!(complete!(hexadecimal))),|v|c_int_radix(v,16)) |
		map_opt!(preceded!(tag!("0b"),many1!(complete!(binary))),|v|c_int_radix(v,2)) |
		map_opt!(preceded!(char!('0'),many1!(complete!(octal))),|v|c_int_radix(v,8)) |
		map_opt!(many1!(complete!(decimal)),|v|c_int_radix(v,10)) |
		force_type!(IResult<_,_,u32>)
	),opt!(take_ul)),|i|i as i64)
);

// ==============================
// ======== parse floats ========
// ==============================

named!(float_width<u8>,complete!(byte!(b'f' | b'l' | b'F' | b'L')));
named!(float_exp<(Option<u8>,Vec<u8>)>,preceded!(byte!(b'e'|b'E'),pair!(opt!(byte!(b'-'|b'+')),many1!(complete!(decimal)))));

named!(c_float<f64>,
	map_opt!(alt!(
		terminated!(recognize!(tuple!(many1!(complete!(decimal)),byte!(b'.'),many0!(complete!(decimal)))),opt!(float_width)) |
		terminated!(recognize!(tuple!(many0!(complete!(decimal)),byte!(b'.'),many1!(complete!(decimal)))),opt!(float_width)) |
		terminated!(recognize!(tuple!(many0!(complete!(decimal)),opt!(byte!(b'.')),many1!(complete!(decimal)),float_exp)),opt!(float_width)) |
		terminated!(recognize!(tuple!(many1!(complete!(decimal)),opt!(byte!(b'.')),many0!(complete!(decimal)),float_exp)),opt!(float_width)) |
		terminated!(recognize!(many1!(complete!(decimal))),float_width)
	),|v|str::from_utf8(v).ok().and_then(|i|f64::from_str(i).ok()))
);

// ================================
// ======== main interface ========
// ================================

named!(one_literal<&[u8],EvalResult,::Error>,
	fix_error!(::Error,alt_complete!(
		map!(full!(c_char),EvalResult::Char) |
		map!(full!(c_int),|i|EvalResult::Int(::std::num::Wrapping(i))) |
		map!(full!(c_float),EvalResult::Float) |
		map!(full!(c_string),EvalResult::Str)
	))
);

/// Parse a C literal.
///
/// The input must contain exactly the representation of a single literal
/// token, and in particular no whitespace or sign prefixes.
pub fn parse(input: &[u8]) -> IResult<&[u8],EvalResult,::Error> {
	::assert_full_parse(one_literal(input))
}
