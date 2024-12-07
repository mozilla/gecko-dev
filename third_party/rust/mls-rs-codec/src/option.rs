// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{MlsDecode, MlsEncode, MlsSize};
use alloc::vec::Vec;

impl<T: MlsSize> MlsSize for Option<T> {
    #[inline]
    fn mls_encoded_len(&self) -> usize {
        1 + match self {
            Some(v) => v.mls_encoded_len(),
            None => 0,
        }
    }
}

impl<T: MlsEncode> MlsEncode for Option<T> {
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), crate::Error> {
        if let Some(item) = self {
            writer.push(1);
            item.mls_encode(writer)
        } else {
            writer.push(0);
            Ok(())
        }
    }
}

impl<T: MlsDecode> MlsDecode for Option<T> {
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, crate::Error> {
        match u8::mls_decode(reader)? {
            0 => Ok(None),
            1 => T::mls_decode(reader).map(Some),
            n => Err(crate::Error::OptionOutOfRange(n)),
        }
    }
}

#[cfg(test)]
mod tests {
    use alloc::vec;

    use crate::{Error, MlsDecode, MlsEncode};
    use assert_matches::assert_matches;

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    #[test]
    fn none_is_serialized_correctly() {
        assert_eq!(vec![0u8], None::<u8>.mls_encode_to_vec().unwrap());
    }

    #[test]
    fn some_is_serialized_correctly() {
        assert_eq!(vec![1u8, 2], Some(2u8).mls_encode_to_vec().unwrap());
    }

    #[test]
    fn none_round_trips() {
        let val = None::<u8>;
        let x = val.mls_encode_to_vec().unwrap();
        assert_eq!(val, Option::mls_decode(&mut &*x).unwrap());
    }

    #[test]
    fn some_round_trips() {
        let val = Some(32u8);
        let x = val.mls_encode_to_vec().unwrap();
        assert_eq!(val, Option::mls_decode(&mut &*x).unwrap());
    }

    #[test]
    fn deserializing_invalid_discriminant_fails() {
        assert_matches!(
            Option::<u8>::mls_decode(&mut &[2u8][..]),
            Err(Error::OptionOutOfRange(_))
        );
    }
}
