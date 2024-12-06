// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{MlsDecode, MlsEncode, MlsSize};
use alloc::vec::Vec;

impl<const N: usize> MlsSize for [u8; N] {
    #[inline(always)]
    fn mls_encoded_len(&self) -> usize {
        N
    }
}

impl<const N: usize> MlsEncode for [u8; N] {
    #[inline(always)]
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), crate::Error> {
        writer.extend_from_slice(self);
        Ok(())
    }
}

impl<const N: usize> MlsDecode for [u8; N] {
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, crate::Error> {
        let array = reader
            .get(..N)
            .and_then(|head| head.try_into().ok())
            .ok_or(crate::Error::UnexpectedEOF)?;

        *reader = &reader[N..];
        Ok(array)
    }
}

#[cfg(test)]
mod tests {
    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    use alloc::vec;

    use crate::{Error, MlsEncode};
    use assert_matches::assert_matches;

    #[test]
    fn serialize_works() {
        let arr = [0u8, 1u8, 2u8];
        assert_eq!(arr.mls_encode_to_vec().unwrap(), vec![0u8, 1u8, 2u8]);
    }

    #[test]
    fn serialize_round_trip() {
        let arr = [0u8, 1u8, 2u8];
        let serialized = arr.mls_encode_to_vec().unwrap();
        let restored: [u8; 3] = crate::MlsDecode::mls_decode(&mut &*serialized).unwrap();
        assert_eq!(arr, restored);
    }

    #[test]
    fn end_of_file_error() {
        let arr = [0u8, 1u8, 2u8];
        let serialized = arr.mls_encode_to_vec().unwrap();
        let res: Result<[u8; 5], Error> = crate::MlsDecode::mls_decode(&mut &*serialized);

        assert_matches!(res, Err(Error::UnexpectedEOF))
    }
}
