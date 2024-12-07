// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{MlsDecode, MlsEncode, MlsSize};

use alloc::vec::Vec;

impl<T, U> MlsSize for (T, U)
where
    T: MlsSize,
    U: MlsSize,
{
    fn mls_encoded_len(&self) -> usize {
        self.0.mls_encoded_len() + self.1.mls_encoded_len()
    }
}

impl<T, U> MlsEncode for (T, U)
where
    T: MlsEncode,
    U: MlsEncode,
{
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), crate::Error> {
        self.0.mls_encode(writer)?;
        self.1.mls_encode(writer)
    }
}

impl<T, U> MlsDecode for (T, U)
where
    T: MlsDecode,
    U: MlsDecode,
{
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, crate::Error> {
        Ok((T::mls_decode(reader)?, U::mls_decode(reader)?))
    }
}
