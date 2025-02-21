// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use core::{
    hash::Hash,
    ops::{Deref, DerefMut},
};

use map_impl::SmallMapInner;
pub use map_impl::{LargeMap, LargeMapEntry, SmallMap};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

#[cfg(feature = "std")]
mod map_impl {
    use core::hash::Hash;
    use std::collections::{hash_map::Entry, HashMap};

    #[derive(Clone, Debug, PartialEq, Eq)]
    #[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
    pub struct SmallMap<K: Hash + Eq, V>(pub(super) HashMap<K, V>);

    pub type LargeMap<K, V> = SmallMap<K, V>;
    pub(super) type SmallMapInner<K, V> = HashMap<K, V>;
    pub type LargeMapEntry<'a, K, V> = Entry<'a, K, V>;
}

#[cfg(not(feature = "std"))]
mod map_impl {
    use core::hash::Hash;

    use alloc::{
        collections::{btree_map::Entry, BTreeMap},
        vec::Vec,
    };
    #[cfg(feature = "by_ref_proposal")]
    use itertools::Itertools;

    #[derive(Clone, Debug, PartialEq, Eq)]
    pub struct SmallMap<K: Hash + Eq, V>(pub(super) Vec<(K, V)>);

    pub type LargeMap<K, V> = BTreeMap<K, V>;
    pub(super) type SmallMapInner<K, V> = Vec<(K, V)>;
    pub type LargeMapEntry<'a, K, V> = Entry<'a, K, V>;

    #[cfg(feature = "by_ref_proposal")]
    impl<K: Hash + Eq, V> SmallMap<K, V> {
        pub fn get(&self, key: &K) -> Option<&V> {
            self.find(key).map(|i| &self.0[i].1)
        }

        pub fn insert(&mut self, key: K, value: V) {
            match self.0.iter_mut().find(|(k, _)| (k == &key)) {
                Some((_, v)) => *v = value,
                None => self.0.push((key, value)),
            }
        }

        pub fn remove(&mut self, key: &K) -> Option<V> {
            self.find(key).map(|i| self.0.remove(i).1)
        }

        fn find(&self, key: &K) -> Option<usize> {
            self.0.iter().position(|(k, _)| k == key)
        }
    }
}

impl<K: Hash + Eq, V> Default for SmallMap<K, V> {
    fn default() -> Self {
        Self(SmallMapInner::new())
    }
}

impl<K: Hash + Eq, V> Deref for SmallMap<K, V> {
    type Target = SmallMapInner<K, V>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<K: Hash + Eq, V> DerefMut for SmallMap<K, V> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl<K, V> MlsDecode for SmallMap<K, V>
where
    K: Hash + Eq + MlsEncode + MlsDecode + MlsSize,
    V: MlsEncode + MlsDecode + MlsSize,
{
    fn mls_decode(reader: &mut &[u8]) -> Result<Self, mls_rs_codec::Error> {
        SmallMapInner::mls_decode(reader).map(Self)
    }
}

impl<K, V> MlsSize for SmallMap<K, V>
where
    K: Hash + Eq + MlsEncode + MlsDecode + MlsSize,
    V: MlsEncode + MlsDecode + MlsSize,
{
    fn mls_encoded_len(&self) -> usize {
        self.0.mls_encoded_len()
    }
}

impl<K, V> MlsEncode for SmallMap<K, V>
where
    K: Hash + Eq + MlsEncode + MlsDecode + MlsSize,
    V: MlsEncode + MlsDecode + MlsSize,
{
    fn mls_encode(&self, writer: &mut Vec<u8>) -> Result<(), mls_rs_codec::Error> {
        self.0.mls_encode(writer)
    }
}
