// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;

use crate::{MlsDecode, MlsEncode, MlsSize};

impl<T> MlsSize for [T]
where
    T: MlsSize,
{
    fn mls_encoded_len(&self) -> usize {
        crate::iter::mls_encoded_len(self.iter())
    }
}

impl<T> MlsSize for Vec<T>
where
    T: MlsSize,
{
    #[inline]
    fn mls_encoded_len(&self) -> usize {
        self.as_slice().mls_encoded_len()
    }
}

impl<T> MlsEncode for [T]
where
    T: MlsEncode,
{
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), crate::Error> {
        crate::iter::mls_encode(self.iter(), writer)
    }
}

impl<T> MlsEncode for Vec<T>
where
    T: MlsEncode,
{
    #[inline]
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), crate::Error> {
        self.as_slice().mls_encode(writer)
    }
}

impl<T> MlsDecode for Vec<T>
where
    T: MlsDecode,
{
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, crate::Error> {
        crate::iter::mls_decode_collection(reader, |data| {
            let mut items = Vec::new();

            while !data.is_empty() {
                items.push(T::mls_decode(data)?);
            }

            Ok(items)
        })
    }
}

#[cfg(test)]
mod tests {
    use crate::{Error, MlsDecode, MlsEncode};
    use alloc::{vec, vec::Vec};
    use assert_matches::assert_matches;

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    #[test]
    fn serialization_works() {
        assert_eq!(
            vec![3, 1, 2, 3],
            vec![1u8, 2, 3].mls_encode_to_vec().unwrap()
        );
    }

    #[test]
    fn data_round_trips() {
        let val = vec![1u8, 2, 3];
        let x = val.mls_encode_to_vec().unwrap();
        assert_eq!(val, Vec::mls_decode(&mut &*x).unwrap());
    }

    #[test]
    fn empty_vec_can_be_deserialized() {
        assert_eq!(Vec::<u8>::new(), Vec::mls_decode(&mut &[0u8][..]).unwrap());
    }

    #[test]
    fn too_few_items_to_deserialize_gives_an_error() {
        assert_matches!(
            Vec::<u8>::mls_decode(&mut &[2, 3][..]),
            Err(Error::UnexpectedEOF)
        );
    }
}
