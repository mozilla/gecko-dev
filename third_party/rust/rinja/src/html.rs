use std::{fmt, str};

#[allow(unused)]
pub(crate) fn write_escaped_str(mut dest: impl fmt::Write, src: &str) -> fmt::Result {
    // This implementation reads one byte after another.
    // It's not very fast, but should work well enough until portable SIMD gets stabilized.

    let mut escaped_buf = ESCAPED_BUF_INIT;
    let mut last = 0;

    for (index, byte) in src.bytes().enumerate() {
        if let Some(escaped) = get_escaped(byte) {
            [escaped_buf[2], escaped_buf[3]] = escaped;
            write_str_if_nonempty(&mut dest, &src[last..index])?;
            // SAFETY: the content of `escaped_buf` is pure ASCII
            dest.write_str(unsafe { str::from_utf8_unchecked(&escaped_buf[..ESCAPED_BUF_LEN]) })?;
            last = index + 1;
        }
    }
    write_str_if_nonempty(&mut dest, &src[last..])
}

#[allow(unused)]
pub(crate) fn write_escaped_char(mut dest: impl fmt::Write, c: char) -> fmt::Result {
    if !c.is_ascii() {
        dest.write_char(c)
    } else if let Some(escaped) = get_escaped(c as u8) {
        let mut escaped_buf = ESCAPED_BUF_INIT;
        [escaped_buf[2], escaped_buf[3]] = escaped;
        // SAFETY: the content of `escaped_buf` is pure ASCII
        dest.write_str(unsafe { str::from_utf8_unchecked(&escaped_buf[..ESCAPED_BUF_LEN]) })
    } else {
        // RATIONALE: `write_char(c)` gets optimized if it is known that `c.is_ascii()`
        dest.write_char(c)
    }
}

/// Returns the decimal representation of the codepoint if the character needs HTML escaping.
#[inline(always)]
fn get_escaped(byte: u8) -> Option<[u8; 2]> {
    match byte {
        MIN_CHAR..=MAX_CHAR => match TABLE.lookup[(byte - MIN_CHAR) as usize] {
            0 => None,
            escaped => Some(escaped.to_ne_bytes()),
        },
        _ => None,
    }
}

#[inline(always)]
fn write_str_if_nonempty(output: &mut impl fmt::Write, input: &str) -> fmt::Result {
    if !input.is_empty() {
        output.write_str(input)
    } else {
        Ok(())
    }
}

/// List of characters that need HTML escaping, not necessarily in ordinal order.
const CHARS: &[u8] = br#""&'<>"#;

/// The character with the lowest codepoint that needs HTML escaping.
const MIN_CHAR: u8 = {
    let mut v = u8::MAX;
    let mut i = 0;
    while i < CHARS.len() {
        if v > CHARS[i] {
            v = CHARS[i];
        }
        i += 1;
    }
    v
};

/// The character with the highest codepoint that needs HTML escaping.
const MAX_CHAR: u8 = {
    let mut v = u8::MIN;
    let mut i = 0;
    while i < CHARS.len() {
        if v < CHARS[i] {
            v = CHARS[i];
        }
        i += 1;
    }
    v
};

/// Number of codepoints between the lowest and highest character that needs escaping, incl.
const CHAR_RANGE: usize = (MAX_CHAR - MIN_CHAR + 1) as usize;

struct Table {
    _align: [usize; 0],
    lookup: [u16; CHAR_RANGE],
}

/// For characters that need HTML escaping, the codepoint is formatted as decimal digits,
/// otherwise `b"\0\0"`. Starting at [`MIN_CHAR`].
const TABLE: Table = {
    let mut table = Table {
        _align: [],
        lookup: [0; CHAR_RANGE],
    };
    let mut i = 0;
    while i < CHARS.len() {
        let c = CHARS[i];
        let h = c / 10 + b'0';
        let l = c % 10 + b'0';
        table.lookup[(c - MIN_CHAR) as usize] = u16::from_ne_bytes([h, l]);
        i += 1;
    }
    table
};

// RATIONALE: llvm generates better code if the buffer is register sized
const ESCAPED_BUF_INIT: [u8; 8] = *b"&#__;\0\0\0";
const ESCAPED_BUF_LEN: usize = b"&#__;".len();

#[test]
fn test_simple_html_string_escaping() {
    let mut buf = String::new();
    write_escaped_str(&mut buf, "<script>").unwrap();
    assert_eq!(buf, "&#60;script&#62;");

    buf.clear();
    write_escaped_str(&mut buf, "s<crip>t").unwrap();
    assert_eq!(buf, "s&#60;crip&#62;t");

    buf.clear();
    write_escaped_str(&mut buf, "s<cripcripcripcripcripcripcripcripcripcrip>t").unwrap();
    assert_eq!(buf, "s&#60;cripcripcripcripcripcripcripcripcripcrip&#62;t");
}
