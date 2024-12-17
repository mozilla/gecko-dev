// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::{
    fmt::{self, Debug},
    ops::Deref,
};

use crate::client::MlsError;
use crate::CipherSuiteProvider;
use alloc::vec::Vec;
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::error::IntoAnyError;

#[derive(MlsSize, MlsEncode)]
struct RefHashInput<'a> {
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    pub label: &'a [u8],
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    pub value: &'a [u8],
}

impl Debug for RefHashInput<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("RefHashInput")
            .field("label", &mls_rs_core::debug::pretty_bytes(self.label))
            .field("value", &mls_rs_core::debug::pretty_bytes(self.value))
            .finish()
    }
}

#[derive(PartialEq, Eq, PartialOrd, Ord, Hash, Clone, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct HashReference(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    Vec<u8>,
);

impl Debug for HashReference {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("HashReference")
            .fmt(f)
    }
}

impl Deref for HashReference {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl AsRef<[u8]> for HashReference {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl From<Vec<u8>> for HashReference {
    fn from(val: Vec<u8>) -> Self {
        Self(val)
    }
}

impl HashReference {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn compute<P: CipherSuiteProvider>(
        value: &[u8],
        label: &[u8],
        cipher_suite: &P,
    ) -> Result<HashReference, MlsError> {
        let input = RefHashInput { label, value };
        let input_bytes = input.mls_encode_to_vec()?;

        cipher_suite
            .hash(&input_bytes)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
            .map(HashReference)
    }
}

#[cfg(test)]
mod tests {
    use crate::crypto::test_utils::try_test_cipher_suite_provider;

    #[cfg(not(mls_build_async))]
    use crate::{cipher_suite::CipherSuite, crypto::test_utils::test_cipher_suite_provider};

    use super::*;
    use alloc::string::String;
    use serde::{Deserialize, Serialize};

    #[cfg(not(mls_build_async))]
    use alloc::string::ToString;

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    #[derive(Debug, Deserialize, Serialize)]
    struct HashRefTestCase {
        label: String,
        #[serde(with = "hex::serde")]
        value: Vec<u8>,
        #[serde(with = "hex::serde")]
        out: Vec<u8>,
    }

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    pub struct InteropTestCase {
        cipher_suite: u16,
        ref_hash: HashRefTestCase,
    }

    #[cfg(not(mls_build_async))]
    #[cfg_attr(coverage_nightly, coverage(off))]
    fn generate_test_vector() -> Vec<InteropTestCase> {
        CipherSuite::all()
            .map(|cipher_suite| {
                let provider = test_cipher_suite_provider(cipher_suite);

                let input = b"test input";
                let label = "test label";

                let output = HashReference::compute(input, label.as_bytes(), &provider).unwrap();

                let ref_hash = HashRefTestCase {
                    label: label.to_string(),
                    value: input.to_vec(),
                    out: output.to_vec(),
                };

                InteropTestCase {
                    cipher_suite: cipher_suite.into(),
                    ref_hash,
                }
            })
            .collect()
    }

    #[cfg(mls_build_async)]
    fn generate_test_vector() -> Vec<InteropTestCase> {
        panic!("Tests cannot be generated in async mode");
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_basic_crypto_test_vectors() {
        // The test vector can be found here https://github.com/mlswg/mls-implementations/blob/main/test-vectors/crypto-basics.json
        let test_cases: Vec<InteropTestCase> =
            load_test_case_json!(basic_crypto, generate_test_vector());

        for test_case in test_cases {
            if let Some(cs) = try_test_cipher_suite_provider(test_case.cipher_suite) {
                let label = test_case.ref_hash.label.as_bytes();
                let value = &test_case.ref_hash.value;
                let computed = HashReference::compute(value, label, &cs).await.unwrap();
                assert_eq!(&*computed, &test_case.ref_hash.out);
            }
        }
    }
}
