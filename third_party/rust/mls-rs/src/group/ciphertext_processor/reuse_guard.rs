// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

use crate::CipherSuiteProvider;

const REUSE_GUARD_SIZE: usize = 4;

#[derive(Clone, Debug, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
pub(crate) struct ReuseGuard([u8; REUSE_GUARD_SIZE]);

impl From<[u8; REUSE_GUARD_SIZE]> for ReuseGuard {
    fn from(value: [u8; REUSE_GUARD_SIZE]) -> Self {
        ReuseGuard(value)
    }
}

impl From<ReuseGuard> for [u8; REUSE_GUARD_SIZE] {
    fn from(value: ReuseGuard) -> Self {
        value.0
    }
}

impl AsRef<[u8]> for ReuseGuard {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl ReuseGuard {
    pub(crate) fn random<P: CipherSuiteProvider>(provider: &P) -> Result<Self, P::Error> {
        let mut data = [0u8; REUSE_GUARD_SIZE];
        provider.random_bytes(&mut data).map(|_| ReuseGuard(data))
    }

    pub(crate) fn apply(&self, nonce: &[u8]) -> Vec<u8> {
        let mut new_nonce = nonce.to_vec();

        new_nonce
            .iter_mut()
            .zip(self.as_ref().iter())
            .for_each(|(nonce_byte, guard_byte)| *nonce_byte ^= guard_byte);

        new_nonce
    }
}

#[cfg(test)]
mod test_utils {
    use alloc::vec::Vec;

    use super::{ReuseGuard, REUSE_GUARD_SIZE};

    impl ReuseGuard {
        pub fn new(guard: Vec<u8>) -> Self {
            let mut data = [0u8; REUSE_GUARD_SIZE];
            data.copy_from_slice(&guard);
            Self(data)
        }
    }
}

#[cfg(test)]
mod tests {
    use alloc::vec::Vec;
    use mls_rs_core::crypto::CipherSuiteProvider;

    use crate::{
        client::test_utils::TEST_CIPHER_SUITE, crypto::test_utils::test_cipher_suite_provider,
    };

    use super::{ReuseGuard, REUSE_GUARD_SIZE};

    #[test]
    fn test_random_generation() {
        let test_guard =
            ReuseGuard::random(&test_cipher_suite_provider(TEST_CIPHER_SUITE)).unwrap();

        (0..1000).for_each(|_| {
            let next = ReuseGuard::random(&test_cipher_suite_provider(TEST_CIPHER_SUITE)).unwrap();
            assert_ne!(next, test_guard);
        })
    }

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    struct TestCase {
        nonce: Vec<u8>,
        guard: [u8; REUSE_GUARD_SIZE],
        result: Vec<u8>,
    }

    #[cfg_attr(coverage_nightly, coverage(off))]
    fn generate_reuse_guard_test_cases() -> Vec<TestCase> {
        let provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        [16, 32]
            .into_iter()
            .map(
                #[cfg_attr(coverage_nightly, coverage(off))]
                |len| {
                    let nonce = provider.random_bytes_vec(len).unwrap();
                    let guard = ReuseGuard::random(&provider).unwrap();

                    let result = guard.apply(&nonce);

                    TestCase {
                        nonce,
                        guard: guard.into(),
                        result,
                    }
                },
            )
            .collect()
    }

    fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(reuse_guard, generate_reuse_guard_test_cases())
    }

    #[test]
    fn test_reuse_guard() {
        let test_cases = load_test_cases();

        for case in test_cases {
            let guard = ReuseGuard::from(case.guard);
            let result = guard.apply(&case.nonce);
            assert_eq!(result, case.result);
        }
    }
}
