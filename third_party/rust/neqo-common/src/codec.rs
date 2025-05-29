// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::fmt::{self, Debug, Formatter, Write};

use crate::hex_with_len;

pub const MAX_VARINT: u64 = (1 << 62) - 1;

/// Decoder is a view into a byte array that has a read offset.  Use it for parsing.
pub struct Decoder<'a> {
    buf: &'a [u8],
    offset: usize,
}

impl<'a> Decoder<'a> {
    /// Make a new view of the provided slice.
    #[must_use]
    pub const fn new(buf: &[u8]) -> Decoder {
        Decoder { buf, offset: 0 }
    }

    /// Get the number of bytes remaining until the end.
    #[must_use]
    pub const fn remaining(&self) -> usize {
        self.buf.len() - self.offset
    }

    /// The number of bytes from the underlying slice that have been decoded.
    #[must_use]
    pub const fn offset(&self) -> usize {
        self.offset
    }

    /// Skip n bytes.
    ///
    /// # Panics
    ///
    /// If the remaining quantity is less than `n`.
    pub fn skip(&mut self, n: usize) {
        assert!(self.remaining() >= n, "insufficient data");
        self.offset += n;
    }

    /// Skip helper that panics if `n` is `None` or not able to fit in `usize`.
    /// Only use this for tests because we panic rather than reporting a result.
    #[cfg(any(test, feature = "test-fixture"))]
    fn skip_inner(&mut self, n: Option<u64>) {
        #[expect(clippy::unwrap_used, reason = "Only used in tests.")]
        self.skip(usize::try_from(n.expect("invalid length")).unwrap());
    }

    /// Skip a vector.  Panics if there isn't enough space.
    /// Only use this for tests because we panic rather than reporting a result.
    #[cfg(any(test, feature = "test-fixture"))]
    pub fn skip_vec(&mut self, n: usize) {
        let len = self.decode_n(n);
        self.skip_inner(len);
    }

    /// Skip a variable length vector.  Panics if there isn't enough space.
    /// Only use this for tests because we panic rather than reporting a result.
    #[cfg(any(test, feature = "test-fixture"))]
    pub fn skip_vvec(&mut self) {
        let len = self.decode_varint();
        self.skip_inner(len);
    }

    /// Provides the next byte without moving the read position.
    #[must_use]
    pub const fn peek_byte(&self) -> Option<u8> {
        if self.remaining() < 1 {
            None
        } else {
            Some(self.buf[self.offset])
        }
    }

    /// Decodes arbitrary data.
    pub fn decode(&mut self, n: usize) -> Option<&'a [u8]> {
        if self.remaining() < n {
            return None;
        }
        let res = &self.buf[self.offset..self.offset + n];
        self.offset += n;
        Some(res)
    }

    #[inline]
    pub(crate) fn decode_n(&mut self, n: usize) -> Option<u64> {
        debug_assert!(n > 0 && n <= 8);
        if self.remaining() < n {
            return None;
        }
        Some(if n == 1 {
            let v = u64::from(self.buf[self.offset]);
            self.offset += 1;
            v
        } else {
            let mut buf = [0; 8];
            buf[8 - n..].copy_from_slice(&self.buf[self.offset..self.offset + n]);
            self.offset += n;
            u64::from_be_bytes(buf)
        })
    }

    /// Decodes a big-endian, unsigned integer value into the target type.
    /// This returns `None` if there is not enough data remaining
    /// or if the conversion to the identified type fails.
    /// Conversion is via `u64`, so failures are impossible for
    /// unsigned integer types: `u8`, `u16`, `u32`, or `u64`.
    /// Signed types will fail if the high bit is set.
    pub fn decode_uint<T: TryFrom<u64>>(&mut self) -> Option<T> {
        let v = self.decode_n(size_of::<T>());
        T::try_from(v?).ok()
    }

    /// Decodes a QUIC varint.
    pub fn decode_varint(&mut self) -> Option<u64> {
        let b1 = self.decode_n(1)?;
        match b1 >> 6 {
            0 => Some(b1),
            1 => Some(((b1 & 0x3f) << 8) | self.decode_n(1)?),
            2 => Some(((b1 & 0x3f) << 24) | self.decode_n(3)?),
            3 => Some(((b1 & 0x3f) << 56) | self.decode_n(7)?),
            _ => unreachable!(),
        }
    }

    /// Decodes the rest of the buffer.  Infallible.
    pub fn decode_remainder(&mut self) -> &'a [u8] {
        let res = &self.buf[self.offset..];
        self.offset = self.buf.len();
        res
    }

    fn decode_checked(&mut self, n: Option<u64>) -> Option<&'a [u8]> {
        if let Ok(l) = usize::try_from(n?) {
            self.decode(l)
        } else {
            // sizeof(usize) < sizeof(u64) and the value is greater than
            // usize can hold. Throw away the rest of the input.
            self.offset = self.buf.len();
            None
        }
    }

    /// Decodes a TLS-style length-prefixed buffer.
    pub fn decode_vec(&mut self, n: usize) -> Option<&'a [u8]> {
        let len = self.decode_n(n);
        self.decode_checked(len)
    }

    /// Decodes a QUIC varint-length-prefixed buffer.
    pub fn decode_vvec(&mut self) -> Option<&'a [u8]> {
        let len = self.decode_varint();
        self.decode_checked(len)
    }
}

// Implement `AsRef` for `Decoder` so that values can be examined without
// moving the cursor.
impl<'a> AsRef<[u8]> for Decoder<'a> {
    fn as_ref(&self) -> &'a [u8] {
        &self.buf[self.offset..]
    }
}

impl Debug for Decoder<'_> {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        f.write_str(&hex_with_len(self.as_ref()))
    }
}

impl<'a> From<&'a [u8]> for Decoder<'a> {
    fn from(buf: &'a [u8]) -> Self {
        Decoder::new(buf)
    }
}

impl<'a, T> From<&'a T> for Decoder<'a>
where
    T: AsRef<[u8]>,
{
    fn from(buf: &'a T) -> Self {
        Decoder::new(buf.as_ref())
    }
}

impl<'b> PartialEq<Decoder<'b>> for Decoder<'_> {
    fn eq(&self, other: &Decoder<'b>) -> bool {
        self.buf == other.buf
    }
}

/// Encoder is good for building data structures.
#[derive(Clone, Default, PartialEq, Eq)]
pub struct Encoder {
    buf: Vec<u8>,
}

impl Encoder {
    /// Static helper function for previewing the results of encoding without doing it.
    ///
    /// # Panics
    ///
    /// When `v` is too large.
    #[must_use]
    pub const fn varint_len(v: u64) -> usize {
        match () {
            () if v < (1 << 6) => 1,
            () if v < (1 << 14) => 2,
            () if v < (1 << 30) => 4,
            () if v < (1 << 62) => 8,
            () => panic!("Varint value too large"),
        }
    }

    /// Static helper to determine how long a varint-prefixed array encodes to.
    ///
    /// # Panics
    ///
    /// When `len` doesn't fit in a `u64`.
    #[must_use]
    pub fn vvec_len(len: usize) -> usize {
        Self::varint_len(u64::try_from(len).expect("usize should fit into u64")) + len
    }

    /// Default construction of an empty buffer.
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Construction of a buffer with a predetermined capacity.
    #[must_use]
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            buf: Vec::with_capacity(capacity),
        }
    }

    /// Get the capacity of the underlying buffer: the number of bytes that can be
    /// written without causing an allocation to occur.
    #[must_use]
    pub fn capacity(&self) -> usize {
        self.buf.capacity()
    }

    /// Get the length of the underlying buffer: the number of bytes that have
    /// been written to the buffer.
    #[must_use]
    pub fn len(&self) -> usize {
        self.buf.len()
    }

    /// Returns true if the encoder buffer contains no elements.
    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.buf.is_empty()
    }

    /// Create a view of the current contents of the buffer.
    /// Note: for a view of a slice, use `Decoder::new(&enc[s..e])`
    #[must_use]
    pub fn as_decoder(&self) -> Decoder {
        Decoder::new(self.as_ref())
    }

    /// Don't use this except in testing.
    ///
    /// # Panics
    ///
    /// When `s` contains non-hex values or an odd number of values.
    #[cfg(any(test, feature = "test-fixture"))]
    #[must_use]
    pub fn from_hex(s: impl AsRef<str>) -> Self {
        let s = s.as_ref();
        assert_eq!(s.len() % 2, 0, "Needs to be even length");

        let cap = s.len() / 2;
        let mut enc = Self::with_capacity(cap);

        for i in 0..cap {
            #[expect(clippy::unwrap_used, reason = "Only used in tests.")]
            let v = u8::from_str_radix(&s[i * 2..i * 2 + 2], 16).unwrap();
            enc.encode_byte(v);
        }
        enc
    }

    /// Generic encode routine for arbitrary data.
    pub fn encode(&mut self, data: &[u8]) -> &mut Self {
        self.buf.extend_from_slice(data.as_ref());
        self
    }

    /// Encode a single byte.
    pub fn encode_byte(&mut self, data: u8) -> &mut Self {
        self.buf.push(data);
        self
    }

    /// Encode an integer of any size up to u64.
    ///
    /// # Panics
    ///
    /// When `n` is outside the range `1..=8`.
    pub fn encode_uint<T: Into<u64>>(&mut self, n: usize, v: T) -> &mut Self {
        let v = v.into();
        assert!(n > 0 && n <= 8);
        for i in 0..n {
            self.encode_byte(((v >> (8 * (n - i - 1))) & 0xff) as u8);
        }
        self
    }

    /// Encode a QUIC varint.
    ///
    /// # Panics
    ///
    /// When `v >= 1<<62`.
    pub fn encode_varint<T: Into<u64>>(&mut self, v: T) -> &mut Self {
        let v = v.into();
        match () {
            () if v < (1 << 6) => self.encode_uint(1, v),
            () if v < (1 << 14) => self.encode_uint(2, v | (1 << 14)),
            () if v < (1 << 30) => self.encode_uint(4, v | (2 << 30)),
            () if v < (1 << 62) => self.encode_uint(8, v | (3 << 62)),
            () => panic!("Varint value too large"),
        };
        self
    }

    /// Encode a vector in TLS style.
    ///
    /// # Panics
    ///
    /// When `v` is longer than 2^64.
    pub fn encode_vec(&mut self, n: usize, v: &[u8]) -> &mut Self {
        self.encode_uint(
            n,
            u64::try_from(v.as_ref().len()).expect("v is longer than 2^64"),
        )
        .encode(v)
    }

    /// Encode a vector in TLS style using a closure for the contents.
    ///
    /// # Panics
    ///
    /// When `f()` returns a length larger than `2^8n`.
    #[expect(
        clippy::cast_possible_truncation,
        reason = "AND'ing with 0xff makes this OK."
    )]
    pub fn encode_vec_with<F: FnOnce(&mut Self)>(&mut self, n: usize, f: F) -> &mut Self {
        let start = self.buf.len();
        self.buf.resize(self.buf.len() + n, 0);
        f(self);
        let len = self.buf.len() - start - n;
        assert!(len < (1 << (n * 8)));
        for i in 0..n {
            self.buf[start + i] = ((len >> (8 * (n - i - 1))) & 0xff) as u8;
        }
        self
    }

    /// Encode a vector with a varint length.
    ///
    /// # Panics
    ///
    /// When `v` is longer than 2^64.
    pub fn encode_vvec(&mut self, v: &[u8]) -> &mut Self {
        self.encode_varint(u64::try_from(v.as_ref().len()).expect("v is longer than 2^64"))
            .encode(v)
    }

    /// Encode a vector with a varint length using a closure.
    ///
    /// # Panics
    ///
    /// When `f()` writes more than 2^62 bytes.
    pub fn encode_vvec_with<F: FnOnce(&mut Self)>(&mut self, f: F) -> &mut Self {
        let start = self.buf.len();
        // Optimize for short buffers, reserve a single byte for the length.
        self.buf.resize(self.buf.len() + 1, 0);
        f(self);
        let len = self.buf.len() - start - 1;

        // Now to insert a varint for `len` before the encoded block.
        //
        // We now have one zero byte at `start`, followed by `len` encoded bytes:
        //   |  0  | ... encoded ... |
        // We are going to encode a varint by putting the low bytes in that spare byte.
        // Any additional bytes for the varint are put after the encoded blob:
        //   | low | ... encoded ... | varint high |
        // Then we will rotate that entire piece right, by however many bytes we add:
        //   | varint high | low | ... encoded ... |
        // As long as encoding more than 63 bytes is rare, this won't cost much relative
        // to the convenience of being able to use this function.

        let v = u64::try_from(len).expect("encoded value fits in a u64");
        // The lower order byte fits before the inserted block of bytes.
        self.buf[start] = (v & 0xff) as u8;
        let (count, bits) = match () {
            // Great.  The byte we have is enough.
            () if v < (1 << 6) => return self,
            () if v < (1 << 14) => (1, 1 << 6),
            () if v < (1 << 30) => (3, 2 << 22),
            () if v < (1 << 62) => (7, 3 << 54),
            () => panic!("Varint value too large"),
        };
        // Now, we need to encode the high bits after the main block, ...
        self.encode_uint(count, (v >> 8) | bits);
        // ..., then rotate the entire thing right by the same amount.
        self.buf[start..].rotate_right(count);
        self
    }

    /// Truncate the encoder to the given size.
    pub fn truncate(&mut self, len: usize) {
        self.buf.truncate(len);
    }

    /// Pad the buffer to `len` with bytes set to `v`.
    pub fn pad_to(&mut self, len: usize, v: u8) {
        if len > self.buf.len() {
            self.buf.resize(len, v);
        }
    }
}

impl Debug for Encoder {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        f.write_str(&hex_with_len(self))
    }
}

impl AsRef<[u8]> for Encoder {
    fn as_ref(&self) -> &[u8] {
        self.buf.as_ref()
    }
}

impl AsMut<[u8]> for Encoder {
    fn as_mut(&mut self) -> &mut [u8] {
        self.buf.as_mut()
    }
}

impl<'a> From<Decoder<'a>> for Encoder {
    fn from(dec: Decoder<'a>) -> Self {
        Self::from(&dec.buf[dec.offset..])
    }
}

impl From<&[u8]> for Encoder {
    fn from(buf: &[u8]) -> Self {
        Self {
            buf: Vec::from(buf),
        }
    }
}

impl From<Encoder> for Vec<u8> {
    fn from(buf: Encoder) -> Self {
        buf.buf
    }
}

impl Write for Encoder {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.buf.extend_from_slice(s.as_bytes());
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::{Decoder, Encoder};

    #[test]
    fn decode() {
        let enc = Encoder::from_hex("012345");
        let mut dec = enc.as_decoder();
        assert_eq!(dec.decode(2).unwrap(), &[0x01, 0x23]);
        assert!(dec.decode(2).is_none());
    }

    #[test]
    fn decode_byte() {
        let enc = Encoder::from_hex("0123");
        let mut dec = enc.as_decoder();

        assert_eq!(dec.decode_uint::<u8>().unwrap(), 0x01);
        assert_eq!(dec.decode_uint::<u8>().unwrap(), 0x23);
        assert!(dec.decode_uint::<u8>().is_none());
    }

    #[test]
    fn peek_byte() {
        let enc = Encoder::from_hex("01");
        let mut dec = enc.as_decoder();

        assert_eq!(dec.offset(), 0);
        assert_eq!(dec.peek_byte().unwrap(), 0x01);
        dec.skip(1);
        assert_eq!(dec.offset(), 1);
        assert!(dec.peek_byte().is_none());
    }

    #[test]
    fn decode_byte_short() {
        let enc = Encoder::from_hex("");
        let mut dec = enc.as_decoder();
        assert!(dec.decode_uint::<u8>().is_none());
    }

    #[test]
    fn decode_remainder() {
        let enc = Encoder::from_hex("012345");
        let mut dec = enc.as_decoder();
        assert_eq!(dec.decode_remainder(), &[0x01, 0x23, 0x45]);
        assert!(dec.decode(2).is_none());

        let mut dec = Decoder::from(&[]);
        assert!(dec.decode_remainder().is_empty());
    }

    #[test]
    fn decode_vec() {
        let enc = Encoder::from_hex("012345");
        let mut dec = enc.as_decoder();
        assert_eq!(dec.decode_vec(1).expect("read one octet length"), &[0x23]);
        assert_eq!(dec.remaining(), 1);

        let enc = Encoder::from_hex("00012345");
        let mut dec = enc.as_decoder();
        assert_eq!(dec.decode_vec(2).expect("read two octet length"), &[0x23]);
        assert_eq!(dec.remaining(), 1);
    }

    #[test]
    fn decode_vec_short() {
        // The length is too short.
        let enc = Encoder::from_hex("02");
        let mut dec = enc.as_decoder();
        assert!(dec.decode_vec(2).is_none());

        // The body is too short.
        let enc = Encoder::from_hex("0200");
        let mut dec = enc.as_decoder();
        assert!(dec.decode_vec(1).is_none());
    }

    #[test]
    fn decode_vvec() {
        let enc = Encoder::from_hex("012345");
        let mut dec = enc.as_decoder();
        assert_eq!(dec.decode_vvec().expect("read one octet length"), &[0x23]);
        assert_eq!(dec.remaining(), 1);

        let enc = Encoder::from_hex("40012345");
        let mut dec = enc.as_decoder();
        assert_eq!(dec.decode_vvec().expect("read two octet length"), &[0x23]);
        assert_eq!(dec.remaining(), 1);
    }

    #[test]
    fn decode_vvec_short() {
        // The length field is too short.
        let enc = Encoder::from_hex("ff");
        let mut dec = enc.as_decoder();
        assert!(dec.decode_vvec().is_none());

        let enc = Encoder::from_hex("405500");
        let mut dec = enc.as_decoder();
        assert!(dec.decode_vvec().is_none());
    }

    #[test]
    fn skip() {
        let enc = Encoder::from_hex("ffff");
        let mut dec = enc.as_decoder();
        dec.skip(1);
        assert_eq!(dec.remaining(), 1);
    }

    #[test]
    #[should_panic(expected = "insufficient data")]
    fn skip_too_much() {
        let enc = Encoder::from_hex("ff");
        let mut dec = enc.as_decoder();
        dec.skip(2);
    }

    #[test]
    fn skip_vec() {
        let enc = Encoder::from_hex("012345");
        let mut dec = enc.as_decoder();
        dec.skip_vec(1);
        assert_eq!(dec.remaining(), 1);
    }

    #[test]
    #[should_panic(expected = "insufficient data")]
    fn skip_vec_too_much() {
        let enc = Encoder::from_hex("ff1234");
        let mut dec = enc.as_decoder();
        dec.skip_vec(1);
    }

    #[test]
    #[should_panic(expected = "invalid length")]
    fn skip_vec_short_length() {
        let enc = Encoder::from_hex("ff");
        let mut dec = enc.as_decoder();
        dec.skip_vec(4);
    }
    #[test]
    fn skip_vvec() {
        let enc = Encoder::from_hex("012345");
        let mut dec = enc.as_decoder();
        dec.skip_vvec();
        assert_eq!(dec.remaining(), 1);
    }

    #[test]
    #[should_panic(expected = "insufficient data")]
    fn skip_vvec_too_much() {
        let enc = Encoder::from_hex("0f1234");
        let mut dec = enc.as_decoder();
        dec.skip_vvec();
    }

    #[test]
    #[should_panic(expected = "invalid length")]
    fn skip_vvec_short_length() {
        let enc = Encoder::from_hex("ff");
        let mut dec = enc.as_decoder();
        dec.skip_vvec();
    }

    #[test]
    fn encoded_lengths() {
        assert_eq!(Encoder::varint_len(0), 1);
        assert_eq!(Encoder::varint_len(0x3f), 1);
        assert_eq!(Encoder::varint_len(0x40), 2);
        assert_eq!(Encoder::varint_len(0x3fff), 2);
        assert_eq!(Encoder::varint_len(0x4000), 4);
        assert_eq!(Encoder::varint_len(0x3fff_ffff), 4);
        assert_eq!(Encoder::varint_len(0x4000_0000), 8);
    }

    #[test]
    #[should_panic(expected = "Varint value too large")]
    const fn encoded_length_oob() {
        _ = Encoder::varint_len(1 << 62);
    }

    #[test]
    fn encoded_vvec_lengths() {
        assert_eq!(Encoder::vvec_len(0), 1);
        assert_eq!(Encoder::vvec_len(0x3f), 0x40);
        assert_eq!(Encoder::vvec_len(0x40), 0x42);
        assert_eq!(Encoder::vvec_len(0x3fff), 0x4001);
        assert_eq!(Encoder::vvec_len(0x4000), 0x4004);
        assert_eq!(Encoder::vvec_len(0x3fff_ffff), 0x4000_0003);
        assert_eq!(Encoder::vvec_len(0x4000_0000), 0x4000_0008);
    }

    #[test]
    #[cfg(target_pointer_width = "64")] // Test does not compile on 32-bit targets.
    #[should_panic(expected = "Varint value too large")]
    fn encoded_vvec_length_oob() {
        _ = Encoder::vvec_len(1 << 62);
    }

    #[test]
    fn encode_byte() {
        let mut enc = Encoder::default();

        enc.encode_byte(1);
        assert_eq!(enc, Encoder::from_hex("01"));

        enc.encode_byte(0xfe);
        assert_eq!(enc, Encoder::from_hex("01fe"));
    }

    #[test]
    fn encode() {
        let mut enc = Encoder::default();
        enc.encode(&[1, 2, 3]);
        assert_eq!(enc, Encoder::from_hex("010203"));
    }

    #[test]
    fn encode_uint() {
        let mut enc = Encoder::default();
        enc.encode_uint(2, 10_u8); // 000a
        enc.encode_uint(1, 257_u16); // 01
        enc.encode_uint(3, 0xff_ffff_u32); // ffffff
        enc.encode_uint(8, 0xfedc_ba98_7654_3210_u64);
        assert_eq!(enc, Encoder::from_hex("000a01fffffffedcba9876543210"));
    }

    #[test]
    fn builder_from_slice() {
        let slice = &[1, 2, 3];
        let enc = Encoder::from(&slice[..]);
        assert_eq!(enc, Encoder::from_hex("010203"));
    }

    #[test]
    fn builder_inas_decoder() {
        let enc = Encoder::from_hex("010203");
        let buf = &[1, 2, 3];
        assert_eq!(enc.as_decoder(), Decoder::new(buf));
    }

    struct UintTestCase {
        v: u64,
        b: String,
    }

    macro_rules! uint_tc {
        [$( $v:expr => $b:expr ),+ $(,)?] => {
            vec![ $( UintTestCase { v: $v, b: String::from($b) } ),+]
        };
    }

    #[test]
    fn varint_encode_decode() {
        let cases = uint_tc![
            0 => "00",
            1 => "01",
            63 => "3f",
            64 => "4040",
            16383 => "7fff",
            16384 => "80004000",
            (1 << 30) - 1 => "bfffffff",
            1 << 30 => "c000000040000000",
            (1 << 62) - 1 => "ffffffffffffffff",
        ];

        for c in cases {
            assert_eq!(Encoder::varint_len(c.v), c.b.len() / 2);

            let mut enc = Encoder::default();
            enc.encode_varint(c.v);
            let encoded = Encoder::from_hex(&c.b);
            assert_eq!(enc, encoded);

            let mut dec = encoded.as_decoder();
            let v = dec.decode_varint().expect("should decode");
            assert_eq!(dec.remaining(), 0);
            assert_eq!(v, c.v);
        }
    }

    #[test]
    fn varint_decode_long_zero() {
        for c in &["4000", "80000000", "c000000000000000"] {
            let encoded = Encoder::from_hex(c);
            let mut dec = encoded.as_decoder();
            let v = dec.decode_varint().expect("should decode");
            assert_eq!(dec.remaining(), 0);
            assert_eq!(v, 0);
        }
    }

    #[test]
    fn varint_decode_short() {
        for c in &["40", "800000", "c0000000000000"] {
            let encoded = Encoder::from_hex(c);
            let mut dec = encoded.as_decoder();
            assert!(dec.decode_varint().is_none());
        }
    }

    #[test]
    fn encode_vec() {
        let mut enc = Encoder::default();
        enc.encode_vec(2, &[1, 2, 0x34]);
        assert_eq!(enc, Encoder::from_hex("0003010234"));
    }

    #[test]
    fn encode_vec_with() {
        let mut enc = Encoder::default();
        enc.encode_vec_with(2, |enc_inner| {
            enc_inner.encode(Encoder::from_hex("02").as_ref());
        });
        assert_eq!(enc, Encoder::from_hex("000102"));
    }

    #[test]
    #[should_panic(expected = "assertion failed")]
    fn encode_vec_with_overflow() {
        let mut enc = Encoder::default();
        enc.encode_vec_with(1, |enc_inner| {
            enc_inner.encode(&[0xb0; 256]);
        });
    }

    #[test]
    fn encode_vvec() {
        let mut enc = Encoder::default();
        enc.encode_vvec(&[1, 2, 0x34]);
        assert_eq!(enc, Encoder::from_hex("03010234"));
    }

    #[test]
    fn encode_vvec_with() {
        let mut enc = Encoder::default();
        enc.encode_vvec_with(|enc_inner| {
            enc_inner.encode(Encoder::from_hex("02").as_ref());
        });
        assert_eq!(enc, Encoder::from_hex("0102"));
    }

    #[test]
    fn encode_vvec_with_longer() {
        let mut enc = Encoder::default();
        enc.encode_vvec_with(|enc_inner| {
            enc_inner.encode(&[0xa5; 65]);
        });
        let v: Vec<u8> = enc.into();
        assert_eq!(&v[..3], &[0x40, 0x41, 0xa5]);
    }

    // Test that Deref to &[u8] works for Encoder.
    #[test]
    fn encode_builder() {
        let mut enc = Encoder::from_hex("ff");
        let enc2 = Encoder::from_hex("010234");
        enc.encode(enc2.as_ref());
        assert_eq!(enc, Encoder::from_hex("ff010234"));
    }

    // Test that Deref to &[u8] works for Decoder.
    #[test]
    fn encode_view() {
        let mut enc = Encoder::from_hex("ff");
        let enc2 = Encoder::from_hex("010234");
        let v = enc2.as_decoder();
        enc.encode(v.as_ref());
        assert_eq!(enc, Encoder::from_hex("ff010234"));
    }

    #[test]
    fn encode_mutate() {
        let mut enc = Encoder::from_hex("010234");
        enc.as_mut()[0] = 0xff;
        assert_eq!(enc, Encoder::from_hex("ff0234"));
    }

    #[test]
    fn pad() {
        let mut enc = Encoder::from_hex("010234");
        enc.pad_to(5, 0);
        assert_eq!(enc, Encoder::from_hex("0102340000"));
        enc.pad_to(4, 0);
        assert_eq!(enc, Encoder::from_hex("0102340000"));
        enc.pad_to(7, 0xc2);
        assert_eq!(enc, Encoder::from_hex("0102340000c2c2"));
    }
}
