// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use zeroize::Zeroizing;

use crate::{crypto::CipherSuiteProvider, group::secret_tree::MessageKeyData};

use super::reuse_guard::ReuseGuard;

#[derive(Debug, PartialEq, Eq)]
pub struct MessageKey(MessageKeyData);

impl MessageKey {
    pub(crate) fn new(key: MessageKeyData) -> MessageKey {
        MessageKey(key)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn encrypt<P: CipherSuiteProvider>(
        &self,
        provider: &P,
        data: &[u8],
        aad: &[u8],
        reuse_guard: &ReuseGuard,
    ) -> Result<Vec<u8>, P::Error> {
        provider
            .aead_seal(
                &self.0.key,
                data,
                Some(aad),
                &reuse_guard.apply(&self.0.nonce),
            )
            .await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn decrypt<P: CipherSuiteProvider>(
        &self,
        provider: &P,
        data: &[u8],
        aad: &[u8],
        reuse_guard: &ReuseGuard,
    ) -> Result<Zeroizing<Vec<u8>>, P::Error> {
        provider
            .aead_open(
                &self.0.key,
                data,
                Some(aad),
                &reuse_guard.apply(&self.0.nonce),
            )
            .await
    }
}

// TODO: Write test vectors
