// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{MlsDecode, MlsEncode, MlsSize};
use alloc::vec::Vec;

impl MlsSize for bool {
    fn mls_encoded_len(&self) -> usize {
        1
    }
}

impl MlsEncode for bool {
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), crate::Error> {
        writer.push(*self as u8);
        Ok(())
    }
}

impl MlsDecode for bool {
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, crate::Error> {
        MlsDecode::mls_decode(reader).map(|i: u8| i != 0)
    }
}

#[cfg(test)]
mod tests {
    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    use crate::{MlsDecode, MlsEncode};

    use alloc::vec;

    #[test]
    fn round_trip() {
        assert_eq!(false.mls_encode_to_vec().unwrap(), vec![0]);
        assert_eq!(true.mls_encode_to_vec().unwrap(), vec![1]);

        let vec = vec![true, true, false];
        let bytes = vec.mls_encode_to_vec().unwrap();
        assert_eq!(vec, Vec::mls_decode(&mut &*bytes).unwrap())
    }
}
