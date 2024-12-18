// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#[cfg(target_has_atomic = "ptr")]
use alloc::sync::Arc;

#[cfg(not(target_has_atomic = "ptr"))]
use portable_atomic_util::Arc;

use core::{
    convert::Infallible,
    fmt::{self, Debug},
};

#[cfg(feature = "std")]
use std::collections::HashMap;

#[cfg(not(feature = "std"))]
use alloc::collections::BTreeMap;
use alloc::vec::Vec;
use mls_rs_core::key_package::{KeyPackageData, KeyPackageStorage};

#[cfg(feature = "std")]
use std::sync::Mutex;

#[cfg(mls_build_async)]
use alloc::boxed::Box;
#[cfg(not(feature = "std"))]
use spin::Mutex;

#[derive(Clone, Default)]
/// In memory key package storage backed by a HashMap.
///
/// All clones of an instance of this type share the same underlying HashMap.
pub struct InMemoryKeyPackageStorage {
    #[cfg(feature = "std")]
    inner: Arc<Mutex<HashMap<Vec<u8>, KeyPackageData>>>,
    #[cfg(not(feature = "std"))]
    inner: Arc<Mutex<BTreeMap<Vec<u8>, KeyPackageData>>>,
}

impl Debug for InMemoryKeyPackageStorage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("InMemoryKeyPackageStorage")
            .field(
                "inner",
                &mls_rs_core::debug::pretty_with(|f| {
                    f.debug_map()
                        .entries(
                            self.lock()
                                .iter()
                                .map(|(k, v)| (mls_rs_core::debug::pretty_bytes(k), v)),
                        )
                        .finish()
                }),
            )
            .finish()
    }
}

impl InMemoryKeyPackageStorage {
    /// Create an empty key package storage.
    pub fn new() -> Self {
        Default::default()
    }

    /// Insert key package data.
    pub fn insert(&self, id: Vec<u8>, pkg: KeyPackageData) {
        self.lock().insert(id, pkg);
    }

    /// Get a key package data by `id`.
    pub fn get(&self, id: &[u8]) -> Option<KeyPackageData> {
        self.lock().get(id).cloned()
    }

    /// Delete key package data by `id`.
    pub fn delete(&self, id: &[u8]) {
        self.lock().remove(id);
    }

    /// Get all key packages that are currently stored.
    pub fn key_packages(&self) -> Vec<(Vec<u8>, KeyPackageData)> {
        self.lock()
            .iter()
            .map(|(k, v)| (k.clone(), v.clone()))
            .collect()
    }

    #[cfg(feature = "std")]
    fn lock(&self) -> std::sync::MutexGuard<'_, HashMap<Vec<u8>, KeyPackageData>> {
        self.inner.lock().unwrap()
    }

    #[cfg(not(feature = "std"))]
    fn lock(&self) -> spin::mutex::MutexGuard<'_, BTreeMap<Vec<u8>, KeyPackageData>> {
        self.inner.lock()
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(mls_build_async, maybe_async::must_be_async)]
impl KeyPackageStorage for InMemoryKeyPackageStorage {
    type Error = Infallible;

    async fn delete(&mut self, id: &[u8]) -> Result<(), Self::Error> {
        (*self).delete(id);
        Ok(())
    }

    async fn insert(&mut self, id: Vec<u8>, pkg: KeyPackageData) -> Result<(), Self::Error> {
        (*self).insert(id, pkg);
        Ok(())
    }

    async fn get(&self, id: &[u8]) -> Result<Option<KeyPackageData>, Self::Error> {
        Ok(self.get(id))
    }
}
