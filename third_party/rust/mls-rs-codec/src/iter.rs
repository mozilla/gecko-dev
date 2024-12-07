// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{MlsDecode, MlsEncode, MlsSize, VarInt};

use alloc::vec::Vec;

pub fn mls_encoded_len<T>(iter: impl Iterator<Item = T>) -> usize
where
    T: MlsSize,
{
    let len = iter.map(|x| x.mls_encoded_len()).sum::<usize>();

    let header_length = VarInt::try_from(len).unwrap_or(VarInt(0)).mls_encoded_len();

    header_length + len
}

#[cfg(feature = "preallocate")]
pub fn mls_encode<I>(iter: I, writer: &mut Vec<u8>) -> Result<(), crate::Error>
where
    I: IntoIterator + Clone,
    I::Item: MlsEncode,
{
    let len = iter
        .clone()
        .into_iter()
        .map(|x| x.mls_encoded_len())
        .sum::<usize>();

    let header_length = VarInt::try_from(len)?;
    header_length.mls_encode(writer)?;

    writer.reserve(len);

    iter.into_iter().try_for_each(|x| x.mls_encode(writer))?;

    Ok(())
}

#[cfg(not(feature = "preallocate"))]
pub fn mls_encode<I>(iter: I, writer: &mut Vec<u8>) -> Result<(), crate::Error>
where
    I: IntoIterator + Clone,
    I::Item: MlsEncode,
{
    let mut buffer = Vec::new();

    iter.into_iter()
        .try_for_each(|x| x.mls_encode(&mut buffer))?;

    let len = VarInt::try_from(buffer.len())?;

    len.mls_encode(writer)?;
    writer.extend(buffer);

    Ok(())
}

pub fn mls_decode_collection<T, F>(reader: &mut &[u8], item_decode: F) -> Result<T, crate::Error>
where
    F: Fn(&mut &[u8]) -> Result<T, crate::Error>,
{
    let (mut data, rest) = mls_decode_split_on_collection(reader)?;

    let items = item_decode(&mut data)?;

    *reader = rest;

    Ok(items)
}

pub fn mls_decode_split_on_collection<'b>(
    reader: &mut &'b [u8],
) -> Result<(&'b [u8], &'b [u8]), crate::Error> {
    let len = VarInt::mls_decode(reader)?.0 as usize;

    if len > reader.len() {
        return Err(crate::Error::UnexpectedEOF);
    }

    Ok(reader.split_at(len))
}
