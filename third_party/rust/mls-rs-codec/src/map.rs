// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::{collections::BTreeMap, vec::Vec};

#[cfg(feature = "std")]
use std::{collections::HashMap, hash::Hash};

use crate::{MlsDecode, MlsEncode, MlsSize};

#[cfg(feature = "std")]
impl<K, V> MlsSize for HashMap<K, V>
where
    K: MlsSize,
    V: MlsSize,
{
    fn mls_encoded_len(&self) -> usize {
        crate::iter::mls_encoded_len(self.iter())
    }
}

#[cfg(feature = "std")]
impl<K, V> MlsEncode for HashMap<K, V>
where
    K: MlsEncode,
    V: MlsEncode,
{
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), crate::Error> {
        crate::iter::mls_encode(self.iter(), writer)
    }
}

#[cfg(feature = "std")]
impl<K, V> MlsDecode for HashMap<K, V>
where
    K: MlsDecode + Hash + Eq,
    V: MlsDecode,
{
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, crate::Error> {
        crate::iter::mls_decode_collection(reader, |data| {
            let mut items = HashMap::new();

            while !data.is_empty() {
                items.insert(K::mls_decode(data)?, V::mls_decode(data)?);
            }

            Ok(items)
        })
    }
}

impl<K, V> MlsSize for BTreeMap<K, V>
where
    K: MlsSize,
    V: MlsSize,
{
    fn mls_encoded_len(&self) -> usize {
        crate::iter::mls_encoded_len(self.iter())
    }
}

impl<K, V> MlsEncode for BTreeMap<K, V>
where
    K: MlsEncode,
    V: MlsEncode,
{
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), crate::Error> {
        crate::iter::mls_encode(self.iter(), writer)
    }
}

impl<K, V> MlsDecode for BTreeMap<K, V>
where
    K: MlsDecode + Eq + Ord,
    V: MlsDecode,
{
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, crate::Error> {
        crate::iter::mls_decode_collection(reader, |data| {
            let mut items = BTreeMap::new();

            while !data.is_empty() {
                items.insert(K::mls_decode(data)?, V::mls_decode(data)?);
            }

            Ok(items)
        })
    }
}
