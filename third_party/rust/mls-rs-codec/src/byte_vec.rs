// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{iter::mls_decode_split_on_collection, Error, MlsEncode, MlsSize, VarInt};

use alloc::vec::Vec;

/// Optimized length calculation for types that can be represented as u8 slices.
pub fn mls_encoded_len<T>(data: &T) -> usize
where
    T: AsRef<[u8]>,
{
    fn slice_len(data: &[u8]) -> usize {
        let len = data.len();
        let header_length = VarInt::try_from(len).unwrap_or(VarInt(0)).mls_encoded_len();

        header_length + len
    }

    slice_len(data.as_ref())
}

/// Optimized encoding for types that can be represented as u8 slices.
pub fn mls_encode<T>(data: &T, writer: &mut Vec<u8>) -> Result<(), Error>
where
    T: AsRef<[u8]>,
{
    fn encode_slice(data: &[u8], writer: &mut Vec<u8>) -> Result<(), Error> {
        let len = VarInt::try_from(data.len())?;

        len.mls_encode(writer)?;
        writer.extend_from_slice(data);

        Ok(())
    }

    encode_slice(data.as_ref(), writer)
}

/// Optimized decoding for types that can be represented as `Vec<u8>`
pub fn mls_decode<T>(reader: &mut &[u8]) -> Result<T, crate::Error>
where
    T: From<Vec<u8>>,
{
    fn decode_vec(reader: &mut &[u8]) -> Result<Vec<u8>, crate::Error> {
        let (data, rest) = mls_decode_split_on_collection(reader)?;

        *reader = rest;

        Ok(data.to_vec())
    }

    let out = decode_vec(reader)?;
    Ok(out.into())
}
