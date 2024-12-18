// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#[cfg(mls_build_async)]
use alloc::boxed::Box;
use alloc::vec::Vec;
use core::fmt::{self, Debug};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

use crate::{crypto::HpkeSecretKey, error::IntoAnyError};

#[derive(Clone, PartialEq, Eq, MlsEncode, MlsDecode, MlsSize)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[non_exhaustive]
/// Representation of a generated key package and secret keys.
pub struct KeyPackageData {
    #[cfg_attr(feature = "serde", serde(with = "crate::vec_serde"))]
    pub key_package_bytes: Vec<u8>,
    pub init_key: HpkeSecretKey,
    pub leaf_node_key: HpkeSecretKey,
    pub expiration: u64,
}

impl Debug for KeyPackageData {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("KeyPackageData")
            .field(
                "key_package_bytes",
                &crate::debug::pretty_bytes(&self.key_package_bytes),
            )
            .field("init_key", &self.init_key)
            .field("leaf_node_key", &self.leaf_node_key)
            .field("expiration", &self.expiration)
            .finish()
    }
}

impl KeyPackageData {
    pub fn new(
        key_package_bytes: Vec<u8>,
        init_key: HpkeSecretKey,
        leaf_node_key: HpkeSecretKey,
        expiration: u64,
    ) -> KeyPackageData {
        Self {
            key_package_bytes,
            init_key,
            leaf_node_key,
            expiration,
        }
    }
}

/// Storage trait that maintains key package secrets.
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(mls_build_async, maybe_async::must_be_async)]
pub trait KeyPackageStorage: Send + Sync {
    /// Error type that the underlying storage mechanism returns on internal
    /// failure.
    type Error: IntoAnyError;

    /// Delete [`KeyPackageData`] referenced by `id`.
    ///
    /// This function is called automatically when the key package referenced
    /// by `id` is used to successfully join a group.
    ///
    /// # Warning
    ///
    /// [`KeyPackageData`] internally contains secret key values. The
    /// provided delete mechanism should securely erase data.
    async fn delete(&mut self, id: &[u8]) -> Result<(), Self::Error>;

    /// Store [`KeyPackageData`] that can be accessed by `id` in the future.
    ///
    /// This function is automatically called whenever a new key package is created.
    async fn insert(&mut self, id: Vec<u8>, pkg: KeyPackageData) -> Result<(), Self::Error>;

    /// Retrieve [`KeyPackageData`] by its `id`.
    ///
    /// `None` should be returned in the event that no key packages are found
    /// that match `id`.
    async fn get(&self, id: &[u8]) -> Result<Option<KeyPackageData>, Self::Error>;
}
