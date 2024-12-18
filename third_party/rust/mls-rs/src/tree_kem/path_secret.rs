// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::client::MlsError;
use crate::crypto::{CipherSuiteProvider, HpkePublicKey, HpkeSecretKey};
use crate::group::key_schedule::kdf_derive_secret;
use alloc::vec;
use alloc::vec::Vec;
use core::{
    fmt::{self, Debug},
    ops::Deref,
};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::error::IntoAnyError;
use zeroize::Zeroizing;

use super::hpke_encryption::HpkeEncryptable;

#[derive(Clone, Eq, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct PathSecret(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::zeroizing_serde"))]
    Zeroizing<Vec<u8>>,
);

impl Debug for PathSecret {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("PathSecret")
            .fmt(f)
    }
}

impl Deref for PathSecret {
    type Target = Vec<u8>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl From<Vec<u8>> for PathSecret {
    fn from(data: Vec<u8>) -> Self {
        PathSecret(Zeroizing::new(data))
    }
}

impl From<Zeroizing<Vec<u8>>> for PathSecret {
    fn from(data: Zeroizing<Vec<u8>>) -> Self {
        PathSecret(data)
    }
}

impl PathSecret {
    pub fn random<P: CipherSuiteProvider>(
        cipher_suite_provider: &P,
    ) -> Result<PathSecret, MlsError> {
        cipher_suite_provider
            .random_bytes_vec(cipher_suite_provider.kdf_extract_size())
            .map(Into::into)
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
    }

    pub fn empty<P: CipherSuiteProvider>(cipher_suite_provider: &P) -> Self {
        // Define commit_secret as the all-zero vector of the same length as a path_secret
        PathSecret::from(vec![0u8; cipher_suite_provider.kdf_extract_size()])
    }
}

impl HpkeEncryptable for PathSecret {
    const ENCRYPT_LABEL: &'static str = "UpdatePathNode";

    fn from_bytes(bytes: Vec<u8>) -> Result<Self, MlsError> {
        Ok(Self(Zeroizing::new(bytes)))
    }

    fn get_bytes(&self) -> Result<Vec<u8>, MlsError> {
        Ok(self.to_vec())
    }
}

impl PathSecret {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn to_hpke_key_pair<P: CipherSuiteProvider>(
        &self,
        cs: &P,
    ) -> Result<(HpkeSecretKey, HpkePublicKey), MlsError> {
        let node_secret = Zeroizing::new(kdf_derive_secret(cs, self, b"node").await?);

        cs.kem_derive(&node_secret)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
    }
}

#[derive(Clone, Debug)]
pub struct PathSecretGenerator<'a, P> {
    cipher_suite_provider: &'a P,
    last: Option<PathSecret>,
    starting_with: Option<PathSecret>,
}

impl<'a, P: CipherSuiteProvider> PathSecretGenerator<'a, P> {
    pub fn new(cipher_suite_provider: &'a P) -> Self {
        Self {
            cipher_suite_provider,
            last: None,
            starting_with: None,
        }
    }

    pub fn starting_with(cipher_suite_provider: &'a P, secret: PathSecret) -> Self {
        Self {
            starting_with: Some(secret),
            ..Self::new(cipher_suite_provider)
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn next_secret(&mut self) -> Result<PathSecret, MlsError> {
        let secret = if let Some(starting_with) = self.starting_with.take() {
            Ok(starting_with)
        } else if let Some(last) = self.last.take() {
            kdf_derive_secret(self.cipher_suite_provider, &last, b"path")
                .await
                .map(PathSecret::from)
        } else {
            PathSecret::random(self.cipher_suite_provider)
        }?;

        self.last = Some(secret.clone());

        Ok(secret)
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        cipher_suite::CipherSuite,
        client::test_utils::TEST_CIPHER_SUITE,
        crypto::test_utils::{
            test_cipher_suite_provider, try_test_cipher_suite_provider, TestCryptoProvider,
        },
    };

    use super::*;

    use alloc::string::String;

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    #[derive(serde::Deserialize, serde::Serialize)]
    struct TestCase {
        cipher_suite: u16,
        generations: Vec<String>,
    }

    impl TestCase {
        #[cfg(not(mls_build_async))]
        #[cfg_attr(coverage_nightly, coverage(off))]
        fn generate() -> Vec<TestCase> {
            CipherSuite::all()
                .map(
                    #[cfg_attr(coverage_nightly, coverage(off))]
                    |cipher_suite| {
                        let cs_provider = test_cipher_suite_provider(cipher_suite);
                        let mut generator = PathSecretGenerator::new(&cs_provider);

                        let generations = (0..10)
                            .map(|_| hex::encode(&*generator.next_secret().unwrap()))
                            .collect();

                        TestCase {
                            cipher_suite: cipher_suite.into(),
                            generations,
                        }
                    },
                )
                .collect()
        }

        #[cfg(mls_build_async)]
        fn generate() -> Vec<TestCase> {
            panic!("Tests cannot be generated in async mode");
        }
    }

    fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(path_secret, TestCase::generate())
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_path_secret_generation() {
        let cases = load_test_cases();

        for test_case in cases {
            let Some(cs_provider) = try_test_cipher_suite_provider(test_case.cipher_suite) else {
                continue;
            };

            let first_secret = PathSecret::from(hex::decode(&test_case.generations[0]).unwrap());
            let mut generator = PathSecretGenerator::starting_with(&cs_provider, first_secret);

            for expected in &test_case.generations {
                let generated = hex::encode(&*generator.next_secret().await.unwrap());
                assert_eq!(expected, &generated);
            }
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_first_path_is_random() {
        let cs_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        let mut generator = PathSecretGenerator::new(&cs_provider);
        let first_secret = generator.next_secret().await.unwrap();

        for _ in 0..100 {
            let mut next_generator = PathSecretGenerator::new(&cs_provider);
            let next_secret = next_generator.next_secret().await.unwrap();
            assert_ne!(first_secret, next_secret);
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_starting_with() {
        let cs_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);
        let secret = PathSecret::random(&cs_provider).unwrap();

        let mut generator = PathSecretGenerator::starting_with(&cs_provider, secret.clone());

        let first_secret = generator.next_secret().await.unwrap();
        let second_secret = generator.next_secret().await.unwrap();

        assert_eq!(secret, first_secret);
        assert_ne!(first_secret, second_secret);
    }

    #[test]
    fn test_empty_path_secret() {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let cs_provider = test_cipher_suite_provider(cipher_suite);
            let empty = PathSecret::empty(&cs_provider);
            assert_eq!(
                empty,
                PathSecret::from(vec![0u8; cs_provider.kdf_extract_size()])
            )
        }
    }

    #[test]
    fn test_random_path_secret() {
        let cs_provider = test_cipher_suite_provider(CipherSuite::P256_AES128);
        let initial = PathSecret::random(&cs_provider).unwrap();

        for _ in 0..100 {
            let next = PathSecret::random(&cs_provider).unwrap();
            assert_ne!(next, initial);
        }
    }
}
