// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use core::fmt::{self, Debug};
use mls_rs_codec::{MlsEncode, MlsSize};
use mls_rs_core::error::IntoAnyError;

use crate::client::MlsError;
use crate::crypto::{CipherSuiteProvider, SignaturePublicKey, SignatureSecretKey};

#[derive(Clone, MlsSize, MlsEncode)]
struct SignContent {
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    label: Vec<u8>,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    content: Vec<u8>,
}

impl Debug for SignContent {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("SignContent")
            .field("label", &mls_rs_core::debug::pretty_bytes(&self.label))
            .field("content", &mls_rs_core::debug::pretty_bytes(&self.content))
            .finish()
    }
}

impl SignContent {
    pub fn new(label: &str, content: Vec<u8>) -> Self {
        Self {
            label: [b"MLS 1.0 ", label.as_bytes()].concat(),
            content,
        }
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(all(target_arch = "wasm32", mls_build_async), maybe_async::must_be_async(?Send))]
#[cfg_attr(
    all(not(target_arch = "wasm32"), mls_build_async),
    maybe_async::must_be_async
)]
pub(crate) trait Signable<'a> {
    const SIGN_LABEL: &'static str;

    type SigningContext: Send + Sync;

    fn signature(&self) -> &[u8];

    fn signable_content(
        &self,
        context: &Self::SigningContext,
    ) -> Result<Vec<u8>, mls_rs_codec::Error>;

    fn write_signature(&mut self, signature: Vec<u8>);

    async fn sign<P: CipherSuiteProvider>(
        &mut self,
        signature_provider: &P,
        signer: &SignatureSecretKey,
        context: &Self::SigningContext,
    ) -> Result<(), MlsError> {
        let sign_content = SignContent::new(Self::SIGN_LABEL, self.signable_content(context)?);

        let signature = signature_provider
            .sign(signer, &sign_content.mls_encode_to_vec()?)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        self.write_signature(signature);

        Ok(())
    }

    async fn verify<P: CipherSuiteProvider>(
        &self,
        signature_provider: &P,
        public_key: &SignaturePublicKey,
        context: &Self::SigningContext,
    ) -> Result<(), MlsError> {
        let sign_content = SignContent::new(Self::SIGN_LABEL, self.signable_content(context)?);

        signature_provider
            .verify(
                public_key,
                self.signature(),
                &sign_content.mls_encode_to_vec()?,
            )
            .await
            .map_err(|_| MlsError::InvalidSignature)
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    use alloc::vec;
    use alloc::{string::String, vec::Vec};
    use mls_rs_core::crypto::CipherSuiteProvider;

    use crate::crypto::test_utils::try_test_cipher_suite_provider;

    use super::Signable;

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    pub struct SignatureInteropTestCase {
        #[serde(with = "hex::serde", rename = "priv")]
        secret: Vec<u8>,
        #[serde(with = "hex::serde", rename = "pub")]
        public: Vec<u8>,
        #[serde(with = "hex::serde")]
        content: Vec<u8>,
        label: String,
        #[serde(with = "hex::serde")]
        signature: Vec<u8>,
    }

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    pub struct InteropTestCase {
        cipher_suite: u16,
        sign_with_label: SignatureInteropTestCase,
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_basic_crypto_test_vectors() {
        let test_cases: Vec<InteropTestCase> =
            load_test_case_json!(basic_crypto, Vec::<InteropTestCase>::new());

        for test_case in test_cases {
            if let Some(cs) = try_test_cipher_suite_provider(test_case.cipher_suite) {
                test_case.sign_with_label.verify(&cs).await;
            }
        }
    }

    pub struct TestSignable {
        pub content: Vec<u8>,
        pub signature: Vec<u8>,
    }

    impl<'a> Signable<'a> for TestSignable {
        const SIGN_LABEL: &'static str = "SignWithLabel";

        type SigningContext = Vec<u8>;

        fn signature(&self) -> &[u8] {
            &self.signature
        }

        fn signable_content(
            &self,
            context: &Self::SigningContext,
        ) -> Result<Vec<u8>, mls_rs_codec::Error> {
            Ok([context.as_slice(), self.content.as_slice()].concat())
        }

        fn write_signature(&mut self, signature: Vec<u8>) {
            self.signature = signature
        }
    }

    impl SignatureInteropTestCase {
        #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
        pub async fn verify<P: CipherSuiteProvider>(&self, cs: &P) {
            let public = self.public.clone().into();

            let signable = TestSignable {
                content: self.content.clone(),
                signature: self.signature.clone(),
            };

            signable.verify(cs, &public, &vec![]).await.unwrap();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{test_utils::TestSignable, *};
    use crate::{
        client::test_utils::TEST_CIPHER_SUITE,
        crypto::test_utils::{
            test_cipher_suite_provider, try_test_cipher_suite_provider, TestCryptoProvider,
        },
        group::test_utils::random_bytes,
    };
    use alloc::vec;
    use assert_matches::assert_matches;

    #[derive(Debug, serde::Serialize, serde::Deserialize)]
    struct TestCase {
        cipher_suite: u16,
        #[serde(with = "hex::serde")]
        content: Vec<u8>,
        #[serde(with = "hex::serde")]
        context: Vec<u8>,
        #[serde(with = "hex::serde")]
        signature: Vec<u8>,
        #[serde(with = "hex::serde")]
        signer: Vec<u8>,
        #[serde(with = "hex::serde")]
        public: Vec<u8>,
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    #[cfg_attr(coverage_nightly, coverage(off))]
    async fn generate_test_cases() -> Vec<TestCase> {
        let mut test_cases = Vec::new();

        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let provider = test_cipher_suite_provider(cipher_suite);

            let (signer, public) = provider.signature_key_generate().await.unwrap();

            let content = random_bytes(32);
            let context = random_bytes(32);

            let mut test_signable = TestSignable {
                content: content.clone(),
                signature: Vec::new(),
            };

            test_signable
                .sign(&provider, &signer, &context)
                .await
                .unwrap();

            test_cases.push(TestCase {
                cipher_suite: cipher_suite.into(),
                content,
                context,
                signature: test_signable.signature,
                signer: signer.to_vec(),
                public: public.to_vec(),
            });
        }

        test_cases
    }

    #[cfg(mls_build_async)]
    async fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(signatures, generate_test_cases().await)
    }

    #[cfg(not(mls_build_async))]
    fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(signatures, generate_test_cases())
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_signatures() {
        let cases = load_test_cases().await;

        for one_case in cases {
            let Some(cipher_suite_provider) = try_test_cipher_suite_provider(one_case.cipher_suite)
            else {
                continue;
            };

            let public_key = SignaturePublicKey::from(one_case.public);

            // Wasm uses incompatible signature secret key format
            #[cfg(not(target_arch = "wasm32"))]
            {
                // Test signature generation
                let mut test_signable = TestSignable {
                    content: one_case.content.clone(),
                    signature: Vec::new(),
                };

                let signature_key = SignatureSecretKey::from(one_case.signer);

                test_signable
                    .sign(&cipher_suite_provider, &signature_key, &one_case.context)
                    .await
                    .unwrap();

                test_signable
                    .verify(&cipher_suite_provider, &public_key, &one_case.context)
                    .await
                    .unwrap();
            }

            // Test verifying an existing signature
            let test_signable = TestSignable {
                content: one_case.content,
                signature: one_case.signature,
            };

            test_signable
                .verify(&cipher_suite_provider, &public_key, &one_case.context)
                .await
                .unwrap();
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_invalid_signature() {
        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        let (correct_secret, _) = cipher_suite_provider
            .signature_key_generate()
            .await
            .unwrap();
        let (_, incorrect_public) = cipher_suite_provider
            .signature_key_generate()
            .await
            .unwrap();

        let mut test_signable = TestSignable {
            content: random_bytes(32),
            signature: vec![],
        };

        test_signable
            .sign(&cipher_suite_provider, &correct_secret, &vec![])
            .await
            .unwrap();

        let res = test_signable
            .verify(&cipher_suite_provider, &incorrect_public, &vec![])
            .await;

        assert_matches!(res, Err(MlsError::InvalidSignature));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_invalid_context() {
        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        let (secret, public) = cipher_suite_provider
            .signature_key_generate()
            .await
            .unwrap();

        let correct_context = random_bytes(32);
        let incorrect_context = random_bytes(32);

        let mut test_signable = TestSignable {
            content: random_bytes(32),
            signature: vec![],
        };

        test_signable
            .sign(&cipher_suite_provider, &secret, &correct_context)
            .await
            .unwrap();

        let res = test_signable
            .verify(&cipher_suite_provider, &public, &incorrect_context)
            .await;

        assert_matches!(res, Err(MlsError::InvalidSignature));
    }
}
