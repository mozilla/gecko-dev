// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::CipherSuiteProvider;
use crate::{client::MlsError, group::transcript_hash::ConfirmedTranscriptHash};
use alloc::vec::Vec;
use core::{
    fmt::{self, Debug},
    ops::Deref,
};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::error::IntoAnyError;

#[derive(Clone, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct ConfirmationTag(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    Vec<u8>,
);

impl Debug for ConfirmationTag {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("ConfirmationTag")
            .fmt(f)
    }
}

impl Deref for ConfirmationTag {
    type Target = Vec<u8>;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl ConfirmationTag {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn create<P: CipherSuiteProvider>(
        confirmation_key: &[u8],
        confirmed_transcript_hash: &ConfirmedTranscriptHash,
        cipher_suite_provider: &P,
    ) -> Result<Self, MlsError> {
        cipher_suite_provider
            .mac(confirmation_key, confirmed_transcript_hash)
            .await
            .map(ConfirmationTag)
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn matches<P: CipherSuiteProvider>(
        &self,
        confirmation_key: &[u8],
        confirmed_transcript_hash: &ConfirmedTranscriptHash,
        cipher_suite_provider: &P,
    ) -> Result<bool, MlsError> {
        let tag = ConfirmationTag::create(
            confirmation_key,
            confirmed_transcript_hash,
            cipher_suite_provider,
        )
        .await?;

        Ok(&tag == self)
    }
}

#[cfg(test)]
impl ConfirmationTag {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn empty<P: CipherSuiteProvider>(cipher_suite_provider: &P) -> Self {
        Self(
            cipher_suite_provider
                .mac(
                    &alloc::vec![0; cipher_suite_provider.kdf_extract_size()],
                    &[],
                )
                .await
                .unwrap(),
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::crypto::test_utils::{test_cipher_suite_provider, TestCryptoProvider};

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_confirmation_tag_matching() {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let cipher_suite_provider = test_cipher_suite_provider(cipher_suite);

            let confirmed_hash_a = ConfirmedTranscriptHash::from(b"foo_a".to_vec());

            let confirmation_key_a = b"bar_a".to_vec();

            let confirmed_hash_b = ConfirmedTranscriptHash::from(b"foo_b".to_vec());

            let confirmation_key_b = b"bar_b".to_vec();

            let confirmation_tag = ConfirmationTag::create(
                &confirmation_key_a,
                &confirmed_hash_a,
                &cipher_suite_provider,
            )
            .await
            .unwrap();

            let matches = confirmation_tag
                .matches(
                    &confirmation_key_a,
                    &confirmed_hash_a,
                    &cipher_suite_provider,
                )
                .await
                .unwrap();

            assert!(matches);

            let matches = confirmation_tag
                .matches(
                    &confirmation_key_b,
                    &confirmed_hash_a,
                    &cipher_suite_provider,
                )
                .await
                .unwrap();

            assert!(!matches);

            let matches = confirmation_tag
                .matches(
                    &confirmation_key_a,
                    &confirmed_hash_b,
                    &cipher_suite_provider,
                )
                .await
                .unwrap();

            assert!(!matches);
        }
    }
}
