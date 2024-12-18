// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec;
use alloc::vec::Vec;
use core::{
    fmt::{self, Debug},
    ops::Deref,
};
use mls_rs_core::crypto::CipherSuiteProvider;
use zeroize::Zeroizing;

#[cfg(feature = "psk")]
use mls_rs_codec::MlsEncode;

#[cfg(feature = "psk")]
use mls_rs_core::{error::IntoAnyError, psk::PreSharedKey};

#[cfg(feature = "psk")]
use crate::{
    client::MlsError,
    group::key_schedule::kdf_expand_with_label,
    psk::{PSKLabel, PreSharedKeyID},
};

#[cfg(feature = "psk")]
#[derive(Clone)]
pub(crate) struct PskSecretInput {
    pub id: PreSharedKeyID,
    pub psk: PreSharedKey,
}

#[derive(PartialEq, Eq, Clone)]
pub(crate) struct PskSecret(Zeroizing<Vec<u8>>);

impl Debug for PskSecret {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("PskSecret")
            .fmt(f)
    }
}

#[cfg(test)]
impl From<Vec<u8>> for PskSecret {
    fn from(value: Vec<u8>) -> Self {
        PskSecret(Zeroizing::new(value))
    }
}

impl Deref for PskSecret {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl PskSecret {
    pub(crate) fn new<P: CipherSuiteProvider>(provider: &P) -> PskSecret {
        PskSecret(Zeroizing::new(vec![0u8; provider.kdf_extract_size()]))
    }

    #[cfg(feature = "psk")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn calculate<P: CipherSuiteProvider>(
        input: &[PskSecretInput],
        cipher_suite_provider: &P,
    ) -> Result<PskSecret, MlsError> {
        let len = u16::try_from(input.len()).map_err(|_| MlsError::TooManyPskIds)?;
        let mut psk_secret = PskSecret::new(cipher_suite_provider);

        for (index, psk_secret_input) in input.iter().enumerate() {
            let index = index as u16;

            let label = PSKLabel {
                id: &psk_secret_input.id,
                index,
                count: len,
            };

            let psk_extracted = cipher_suite_provider
                .kdf_extract(
                    &vec![0; cipher_suite_provider.kdf_extract_size()],
                    &psk_secret_input.psk,
                )
                .await
                .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

            let psk_input = kdf_expand_with_label(
                cipher_suite_provider,
                &psk_extracted,
                b"derived psk",
                &label.mls_encode_to_vec()?,
                None,
            )
            .await?;

            psk_secret = cipher_suite_provider
                .kdf_extract(&psk_input, &psk_secret)
                .await
                .map(PskSecret)
                .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;
        }

        Ok(psk_secret)
    }
}

#[cfg(feature = "psk")]
#[cfg(test)]
mod tests {
    use alloc::vec::Vec;
    #[cfg(not(mls_build_async))]
    use core::iter;
    use serde::{Deserialize, Serialize};

    use crate::{
        crypto::test_utils::try_test_cipher_suite_provider,
        psk::ExternalPskId,
        psk::{JustPreSharedKeyID, PreSharedKeyID, PskNonce},
        CipherSuiteProvider,
    };

    #[cfg(not(mls_build_async))]
    use crate::{
        crypto::test_utils::test_cipher_suite_provider, psk::test_utils::make_external_psk_id,
        CipherSuite,
    };

    use super::{PskSecret, PskSecretInput};

    #[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
    struct PskInfo {
        #[serde(with = "hex::serde")]
        id: Vec<u8>,
        #[serde(with = "hex::serde")]
        psk: Vec<u8>,
        #[serde(with = "hex::serde")]
        nonce: Vec<u8>,
    }

    impl From<PskInfo> for PskSecretInput {
        fn from(info: PskInfo) -> Self {
            let id = PreSharedKeyID {
                key_id: JustPreSharedKeyID::External(ExternalPskId::new(info.id)),
                psk_nonce: PskNonce(info.nonce),
            };

            PskSecretInput {
                id,
                psk: info.psk.into(),
            }
        }
    }

    #[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
    struct TestScenario {
        cipher_suite: u16,
        psks: Vec<PskInfo>,
        #[serde(with = "hex::serde")]
        psk_secret: Vec<u8>,
    }

    impl TestScenario {
        #[cfg_attr(coverage_nightly, coverage(off))]
        #[cfg(not(mls_build_async))]
        fn make_psk_list<CS: CipherSuiteProvider>(cs: &CS, n: usize) -> Vec<PskInfo> {
            iter::repeat_with(
                #[cfg_attr(coverage_nightly, coverage(off))]
                || PskInfo {
                    id: make_external_psk_id(cs).to_vec(),
                    psk: cs.random_bytes_vec(cs.kdf_extract_size()).unwrap(),
                    nonce: crate::psk::test_utils::make_nonce(cs.cipher_suite()).0,
                },
            )
            .take(n)
            .collect::<Vec<_>>()
        }

        #[cfg(not(mls_build_async))]
        #[cfg_attr(coverage_nightly, coverage(off))]
        fn generate() -> Vec<TestScenario> {
            CipherSuite::all()
                .flat_map(
                    #[cfg_attr(coverage_nightly, coverage(off))]
                    |cs| (1..=10).map(move |n| (cs, n)),
                )
                .map(
                    #[cfg_attr(coverage_nightly, coverage(off))]
                    |(cs, n)| {
                        let provider = test_cipher_suite_provider(cs);
                        let psks = Self::make_psk_list(&provider, n);
                        let psk_secret = Self::compute_psk_secret(&provider, psks.clone());
                        TestScenario {
                            cipher_suite: cs.into(),
                            psks: psks.to_vec(),
                            psk_secret: psk_secret.to_vec(),
                        }
                    },
                )
                .collect()
        }

        #[cfg(mls_build_async)]
        fn generate() -> Vec<TestScenario> {
            panic!("Tests cannot be generated in async mode");
        }

        #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
        async fn compute_psk_secret<P: CipherSuiteProvider>(
            provider: &P,
            psks: Vec<PskInfo>,
        ) -> PskSecret {
            let input = psks
                .into_iter()
                .map(PskSecretInput::from)
                .collect::<Vec<_>>();

            PskSecret::calculate(&input, provider).await.unwrap()
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn expected_psk_secret_is_produced() {
        let scenarios: Vec<TestScenario> =
            load_test_case_json!(psk_secret, TestScenario::generate());

        for scenario in scenarios {
            if let Some(provider) = try_test_cipher_suite_provider(scenario.cipher_suite) {
                let computed =
                    TestScenario::compute_psk_secret(&provider, scenario.psks.clone()).await;

                assert_eq!(scenario.psk_secret, computed.to_vec());
            }
        }
    }
}
