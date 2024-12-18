// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_crypto_traits::KdfType;

use alloc::vec::Vec;
use core::fmt::{self, Debug};

#[derive(Clone, Eq, PartialEq)]
pub(crate) struct HpkeKdf<K: KdfType> {
    suite_id: Vec<u8>,
    kdf: K,
}

impl<K: KdfType + Debug> Debug for HpkeKdf<K> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("HpkeKdf")
            .field(
                "suite_id",
                &mls_rs_core::debug::pretty_bytes(&self.suite_id),
            )
            .field("kdf", &self.kdf)
            .finish()
    }
}

impl<K: KdfType> HpkeKdf<K> {
    pub fn new(suite_id: Vec<u8>, kdf: K) -> Self {
        Self { suite_id, kdf }
    }

    pub fn extract_size(&self) -> usize {
        self.kdf.extract_size()
    }

    /* draft-irtf-cfrg-hpke-09 section 4 Cryptographic Dependencies */
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn labeled_extract(
        &self,
        salt: &[u8],
        label: &[u8],
        ikm: &[u8],
    ) -> Result<Vec<u8>, <K as KdfType>::Error> {
        self.kdf
            .extract(
                salt,
                &[b"HPKE-v1" as &[u8], &self.suite_id, label, ikm].concat(),
            )
            .await
    }

    /* draft-irtf-cfrg-hpke-09 section 4 Cryptographic Dependencies */
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn labeled_expand(
        &self,
        key: &[u8],
        label: &[u8],
        info: &[u8],
        len: usize,
    ) -> Result<Vec<u8>, <K as KdfType>::Error> {
        let labeled_info = [
            &(len as u16).to_be_bytes() as &[u8],
            b"HPKE-v1",
            &self.suite_id,
            label,
            info,
        ]
        .concat();

        self.kdf.expand(key, &labeled_info, len).await
    }

    /* draft-irtf-cfrg-hpke-09 section 4.1 DH-Based KEM */
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn labeled_extract_then_expand(
        &self,
        ikm: &[u8],
        ctx: &[u8],
        len: usize,
    ) -> Result<Vec<u8>, <K as KdfType>::Error> {
        let eae_prk = self.labeled_extract(&[], b"eae_prk", ikm).await?;

        self.labeled_expand(&eae_prk, b"shared_secret", ctx, len)
            .await
    }
}
