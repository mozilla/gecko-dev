// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{Error, MlsDecode, MlsEncode, MlsSize};
use alloc::vec::Vec;

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct VarInt(pub u32);

impl VarInt {
    pub const MAX: VarInt = VarInt((1 << 30) - 1);
}

impl From<VarInt> for u32 {
    #[inline]
    fn from(n: VarInt) -> u32 {
        n.0
    }
}

impl TryFrom<u32> for VarInt {
    type Error = Error;

    fn try_from(n: u32) -> Result<Self, Error> {
        (n <= u32::from(VarInt::MAX))
            .then_some(VarInt(n))
            .ok_or(Error::VarIntOutOfRange)
    }
}

impl TryFrom<usize> for VarInt {
    type Error = Error;

    fn try_from(n: usize) -> Result<Self, Error> {
        u32::try_from(n)
            .map_err(|_| Error::VarIntOutOfRange)?
            .try_into()
    }
}

impl MlsSize for VarInt {
    #[inline]
    fn mls_encoded_len(&self) -> usize {
        count_bytes_to_encode_int(*self) as usize
    }
}

impl MlsEncode for VarInt {
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), Error> {
        let mut bytes = self.0.to_be_bytes();

        let bytes = match count_bytes_to_encode_int(*self) {
            LengthEncoding::One => &bytes[3..],
            LengthEncoding::Two => {
                bytes[2] |= 0x40;
                &bytes[2..]
            }
            LengthEncoding::Four => {
                bytes[0] |= 0x80;
                &bytes
            }
        };

        writer.extend_from_slice(bytes);
        Ok(())
    }
}

impl MlsDecode for VarInt {
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, Error> {
        let first = u8::mls_decode(reader)?;

        let prefix = first >> 6;

        let count = (prefix < 3)
            .then_some(1 << prefix)
            .ok_or(Error::InvalidVarIntPrefix(prefix))?;

        let n = (1..count).try_fold(u32::from(first & 0x3f), |n, _| {
            u8::mls_decode(reader).map(|b| n << 8 | u32::from(b))
        })?;

        let n = VarInt(n);

        if count_bytes_to_encode_int(n) as usize == count {
            Ok(n)
        } else {
            Err(Error::VarIntMinimumLengthEncoding)
        }
    }
}

/// Number of bytes to encode a variable-size integer.
#[derive(Debug)]
enum LengthEncoding {
    One = 1,
    Two = 2,
    Four = 4,
}

fn count_bytes_to_encode_int(n: VarInt) -> LengthEncoding {
    let used_bits = 32 - n.0.leading_zeros();
    match used_bits {
        0..=6 => LengthEncoding::One,
        7..=14 => LengthEncoding::Two,
        15..=30 => LengthEncoding::Four,
        _ => panic!("Such a large VarInt cannot be instantiated"),
    }
}

#[cfg(test)]
mod tests {
    use super::VarInt;
    use crate::{Error, MlsDecode, MlsEncode};
    use assert_matches::assert_matches;

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    #[test]
    fn zero_is_convertible_to_varint() {
        assert_matches!(VarInt::try_from(0u32).map(u32::from), Ok(0));
    }

    #[test]
    fn successor_of_max_varint_is_not_convertible_to_varint() {
        let n = u32::from(VarInt::MAX) + 1;
        assert_matches!(VarInt::try_from(n), Err(Error::VarIntOutOfRange));
    }

    #[test]
    fn zero_serializes_as_single_null_byte() {
        assert_eq!(
            VarInt::try_from(0u32).unwrap().mls_encode_to_vec().unwrap(),
            [0]
        );
    }

    #[test]
    fn zero_roundtrips() {
        let n = VarInt::try_from(0u32).unwrap();

        let serialized = n.mls_encode_to_vec().unwrap();
        let restored = VarInt::mls_decode(&mut &*serialized).unwrap();

        assert_eq!(restored, n);
    }

    #[test]
    fn varint_max_roundtrips() {
        let n = VarInt::MAX;

        let serialized = n.mls_encode_to_vec().unwrap();
        let restored = VarInt::mls_decode(&mut &*serialized).unwrap();

        assert_eq!(restored, n);
    }

    fn decoding_matches_rfc(encoded: u32, decoded: u32) {
        let bytes = encoded.to_be_bytes();

        let start = bytes
            .iter()
            .position(|&b| b != 0)
            .unwrap_or(bytes.len() - 1);

        let bytes = &bytes[start..];

        assert_eq!(
            VarInt::mls_decode(&mut &*bytes).unwrap(),
            VarInt::try_from(decoded).unwrap()
        );
    }

    #[test]
    fn decoding_0x25_matches_rfc_result() {
        decoding_matches_rfc(0x25, 37);
    }

    #[test]
    fn decoding_0x7bbd_matches_rfc_result() {
        decoding_matches_rfc(0x7bbd, 15293);
    }

    #[test]
    fn decoding_0x9d7f3e7d_matches_rfc_result() {
        decoding_matches_rfc(0x9d7f3e7d, 494878333);
    }
}
