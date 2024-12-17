// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use core::fmt::{self, Debug};
use mls_rs_codec::{MlsEncode, MlsSize};
use mls_rs_core::{
    crypto::{CipherSuiteProvider, HpkeCiphertext, HpkePublicKey, HpkeSecretKey},
    error::IntoAnyError,
};
use zeroize::Zeroizing;

use crate::client::MlsError;

#[derive(Clone, MlsSize, MlsEncode)]
struct EncryptContext<'a> {
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    label: Vec<u8>,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    context: &'a [u8],
}

impl Debug for EncryptContext<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("EncryptContext")
            .field("label", &mls_rs_core::debug::pretty_bytes(&self.label))
            .field("context", &mls_rs_core::debug::pretty_bytes(self.context))
            .finish()
    }
}

impl<'a> EncryptContext<'a> {
    pub fn new(label: &str, context: &'a [u8]) -> Self {
        Self {
            label: [b"MLS 1.0 ", label.as_bytes()].concat(),
            context,
        }
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(all(target_arch = "wasm32", mls_build_async), maybe_async::must_be_async(?Send))]
#[cfg_attr(
    all(not(target_arch = "wasm32"), mls_build_async),
    maybe_async::must_be_async
)]

pub(crate) trait HpkeEncryptable: Sized {
    const ENCRYPT_LABEL: &'static str;

    async fn encrypt<P: CipherSuiteProvider>(
        &self,
        cipher_suite_provider: &P,
        public_key: &HpkePublicKey,
        context: &[u8],
    ) -> Result<HpkeCiphertext, MlsError> {
        let context = EncryptContext::new(Self::ENCRYPT_LABEL, context)
            .mls_encode_to_vec()
            .map(Zeroizing::new)?;

        let content = self.get_bytes().map(Zeroizing::new)?;

        cipher_suite_provider
            .hpke_seal(public_key, &context, None, &content)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
    }

    async fn decrypt<P: CipherSuiteProvider>(
        cipher_suite_provider: &P,
        secret_key: &HpkeSecretKey,
        public_key: &HpkePublicKey,
        context: &[u8],
        ciphertext: &HpkeCiphertext,
    ) -> Result<Self, MlsError> {
        let context = EncryptContext::new(Self::ENCRYPT_LABEL, context).mls_encode_to_vec()?;

        let plaintext = cipher_suite_provider
            .hpke_open(ciphertext, secret_key, public_key, &context, None)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        Self::from_bytes(plaintext.to_vec())
    }

    fn from_bytes(bytes: Vec<u8>) -> Result<Self, MlsError>;
    fn get_bytes(&self) -> Result<Vec<u8>, MlsError>;
}

#[cfg(test)]
pub(crate) mod test_utils {
    use alloc::{string::String, vec::Vec};
    use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
    use mls_rs_core::crypto::{CipherSuiteProvider, HpkeCiphertext};

    use crate::{client::MlsError, crypto::test_utils::try_test_cipher_suite_provider};

    use super::HpkeEncryptable;

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    pub struct HpkeInteropTestCase {
        #[serde(with = "hex::serde", rename = "priv")]
        secret: Vec<u8>,
        #[serde(with = "hex::serde", rename = "pub")]
        public: Vec<u8>,
        label: String,
        #[serde(with = "hex::serde")]
        context: Vec<u8>,
        #[serde(with = "hex::serde")]
        plaintext: Vec<u8>,
        #[serde(with = "hex::serde")]
        kem_output: Vec<u8>,
        #[serde(with = "hex::serde")]
        ciphertext: Vec<u8>,
    }

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    pub struct InteropTestCase {
        cipher_suite: u16,
        encrypt_with_label: HpkeInteropTestCase,
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_basic_crypto_test_vectors() {
        // The test vector can be found here https://github.com/mlswg/mls-implementations/blob/main/test-vectors/crypto-basics.json
        let test_cases: Vec<InteropTestCase> =
            load_test_case_json!(basic_crypto, Vec::<InteropTestCase>::new());

        for test_case in test_cases {
            if let Some(cs) = try_test_cipher_suite_provider(test_case.cipher_suite) {
                test_case.encrypt_with_label.verify(&cs).await
            }
        }
    }

    #[derive(Clone, Debug, MlsSize, MlsEncode, MlsDecode)]
    struct TestEncryptable(#[mls_codec(with = "mls_rs_codec::byte_vec")] Vec<u8>);

    impl HpkeEncryptable for TestEncryptable {
        const ENCRYPT_LABEL: &'static str = "EncryptWithLabel";

        fn from_bytes(bytes: Vec<u8>) -> Result<Self, MlsError> {
            Ok(Self(bytes))
        }

        #[cfg_attr(coverage_nightly, coverage(off))]
        fn get_bytes(&self) -> Result<Vec<u8>, MlsError> {
            Ok(self.0.clone())
        }
    }

    impl HpkeInteropTestCase {
        #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
        pub async fn verify<P: CipherSuiteProvider>(&self, cs: &P) {
            let secret = self.secret.clone().into();
            let public = self.public.clone().into();

            let ciphertext = HpkeCiphertext {
                kem_output: self.kem_output.clone(),
                ciphertext: self.ciphertext.clone(),
            };

            let computed_plaintext =
                TestEncryptable::decrypt(cs, &secret, &public, &self.context, &ciphertext)
                    .await
                    .unwrap();

            assert_eq!(&computed_plaintext.0, &self.plaintext)
        }
    }
}
