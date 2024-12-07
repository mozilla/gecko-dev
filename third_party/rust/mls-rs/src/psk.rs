// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;

#[cfg(any(test, feature = "external_client"))]
use alloc::vec;

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

#[cfg(any(test, feature = "external_client"))]
use mls_rs_core::psk::PreSharedKeyStorage;

#[cfg(any(test, feature = "external_client"))]
use core::convert::Infallible;
use core::fmt::{self, Debug};

#[cfg(feature = "psk")]
use crate::{client::MlsError, CipherSuiteProvider};

#[cfg(feature = "psk")]
use mls_rs_core::error::IntoAnyError;

#[cfg(feature = "psk")]
pub(crate) mod resolver;
pub(crate) mod secret;

pub use mls_rs_core::psk::{ExternalPskId, PreSharedKey};

#[derive(Clone, Debug, Eq, Hash, PartialEq, PartialOrd, Ord, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct PreSharedKeyID {
    pub key_id: JustPreSharedKeyID,
    pub psk_nonce: PskNonce,
}

impl PreSharedKeyID {
    #[cfg(feature = "psk")]
    pub(crate) fn new<P: CipherSuiteProvider>(
        key_id: JustPreSharedKeyID,
        cs: &P,
    ) -> Result<Self, MlsError> {
        Ok(Self {
            key_id,
            psk_nonce: PskNonce::random(cs)
                .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?,
        })
    }
}

#[derive(Clone, Debug, Eq, Hash, Ord, PartialOrd, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(u8)]
pub(crate) enum JustPreSharedKeyID {
    External(ExternalPskId) = 1u8,
    Resumption(ResumptionPsk) = 2u8,
}

#[derive(Clone, Eq, Hash, Ord, PartialOrd, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct PskGroupId(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    pub Vec<u8>,
);

impl Debug for PskGroupId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("PskGroupId")
            .fmt(f)
    }
}

#[derive(Clone, Eq, Hash, PartialEq, PartialOrd, Ord, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct PskNonce(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    pub Vec<u8>,
);

impl Debug for PskNonce {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("PskNonce")
            .fmt(f)
    }
}

#[cfg(feature = "psk")]
impl PskNonce {
    pub fn random<P: CipherSuiteProvider>(
        cipher_suite_provider: &P,
    ) -> Result<Self, <P as CipherSuiteProvider>::Error> {
        Ok(Self(cipher_suite_provider.random_bytes_vec(
            cipher_suite_provider.kdf_extract_size(),
        )?))
    }
}

#[derive(Clone, Debug, Eq, Hash, Ord, PartialOrd, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct ResumptionPsk {
    pub usage: ResumptionPSKUsage,
    pub psk_group_id: PskGroupId,
    pub psk_epoch: u64,
}

#[derive(Clone, Debug, Eq, Hash, PartialEq, Ord, PartialOrd, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(u8)]
pub(crate) enum ResumptionPSKUsage {
    Application = 1u8,
    Reinit = 2u8,
    Branch = 3u8,
}

#[cfg(feature = "psk")]
#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode)]
struct PSKLabel<'a> {
    id: &'a PreSharedKeyID,
    index: u16,
    count: u16,
}

#[cfg(any(test, feature = "external_client"))]
#[derive(Clone, Copy, Debug)]
pub(crate) struct AlwaysFoundPskStorage;

#[cfg(any(test, feature = "external_client"))]
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(mls_build_async, maybe_async::must_be_async)]
impl PreSharedKeyStorage for AlwaysFoundPskStorage {
    type Error = Infallible;

    async fn get(&self, _: &ExternalPskId) -> Result<Option<PreSharedKey>, Self::Error> {
        Ok(Some(vec![].into()))
    }
}

#[cfg(feature = "psk")]
#[cfg(test)]
pub(crate) mod test_utils {
    use crate::crypto::test_utils::test_cipher_suite_provider;

    use super::PskNonce;
    use mls_rs_core::crypto::CipherSuite;

    #[cfg(not(mls_build_async))]
    use mls_rs_core::{crypto::CipherSuiteProvider, psk::ExternalPskId};

    #[cfg_attr(coverage_nightly, coverage(off))]
    #[cfg(not(mls_build_async))]
    pub(crate) fn make_external_psk_id<P: CipherSuiteProvider>(
        cipher_suite_provider: &P,
    ) -> ExternalPskId {
        ExternalPskId::new(
            cipher_suite_provider
                .random_bytes_vec(cipher_suite_provider.kdf_extract_size())
                .unwrap(),
        )
    }

    pub(crate) fn make_nonce(cipher_suite: CipherSuite) -> PskNonce {
        PskNonce::random(&test_cipher_suite_provider(cipher_suite)).unwrap()
    }
}

#[cfg(feature = "psk")]
#[cfg(test)]
mod tests {
    use crate::crypto::test_utils::TestCryptoProvider;
    use core::iter;

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    use super::test_utils::make_nonce;

    #[test]
    fn random_generation_of_nonces_is_random() {
        let good = TestCryptoProvider::all_supported_cipher_suites()
            .into_iter()
            .all(|cipher_suite| {
                let nonce = make_nonce(cipher_suite);
                iter::repeat_with(|| make_nonce(cipher_suite))
                    .take(1000)
                    .all(|other| other != nonce)
            });

        assert!(good);
    }
}
