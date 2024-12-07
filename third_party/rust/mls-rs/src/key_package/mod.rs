// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::cipher_suite::CipherSuite;
use crate::client::MlsError;
use crate::crypto::HpkePublicKey;
use crate::hash_reference::HashReference;
use crate::identity::SigningIdentity;
use crate::protocol_version::ProtocolVersion;
use crate::signer::Signable;
use crate::tree_kem::leaf_node::{LeafNode, LeafNodeSource};
use crate::CipherSuiteProvider;
use alloc::vec::Vec;
use core::{
    fmt::{self, Debug},
    ops::Deref,
};
use mls_rs_codec::MlsDecode;
use mls_rs_codec::MlsEncode;
use mls_rs_codec::MlsSize;
use mls_rs_core::extension::ExtensionList;

mod validator;
pub(crate) use validator::*;

pub(crate) mod generator;
pub(crate) use generator::*;

#[non_exhaustive]
#[derive(Clone, MlsSize, MlsEncode, MlsDecode, PartialEq)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct KeyPackage {
    pub version: ProtocolVersion,
    pub cipher_suite: CipherSuite,
    pub hpke_init_key: HpkePublicKey,
    pub(crate) leaf_node: LeafNode,
    pub extensions: ExtensionList,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    pub signature: Vec<u8>,
}

impl Debug for KeyPackage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("KeyPackage")
            .field("version", &self.version)
            .field("cipher_suite", &self.cipher_suite)
            .field("hpke_init_key", &self.hpke_init_key)
            .field("leaf_node", &self.leaf_node)
            .field("extensions", &self.extensions)
            .field(
                "signature",
                &mls_rs_core::debug::pretty_bytes(&self.signature),
            )
            .finish()
    }
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Hash, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
pub struct KeyPackageRef(HashReference);

impl Deref for KeyPackageRef {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl From<Vec<u8>> for KeyPackageRef {
    fn from(v: Vec<u8>) -> Self {
        Self(HashReference::from(v))
    }
}

#[derive(MlsSize, MlsEncode)]
struct KeyPackageData<'a> {
    pub version: ProtocolVersion,
    pub cipher_suite: CipherSuite,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    pub hpke_init_key: &'a HpkePublicKey,
    pub leaf_node: &'a LeafNode,
    pub extensions: &'a ExtensionList,
}

// #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen)]
impl KeyPackage {
    #[cfg(feature = "ffi")]
    pub fn version(&self) -> ProtocolVersion {
        self.version
    }

    #[cfg(feature = "ffi")]
    pub fn cipher_suite(&self) -> CipherSuite {
        self.cipher_suite
    }

    pub fn signing_identity(&self) -> &SigningIdentity {
        &self.leaf_node.signing_identity
    }

    // #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen_ignore)]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn to_reference<CP: CipherSuiteProvider>(
        &self,
        cipher_suite_provider: &CP,
    ) -> Result<KeyPackageRef, MlsError> {
        if cipher_suite_provider.cipher_suite() != self.cipher_suite {
            return Err(MlsError::CipherSuiteMismatch);
        }

        Ok(KeyPackageRef(
            HashReference::compute(
                &self.mls_encode_to_vec()?,
                b"MLS 1.0 KeyPackage Reference",
                cipher_suite_provider,
            )
            .await?,
        ))
    }

    pub fn expiration(&self) -> Result<u64, MlsError> {
        if let LeafNodeSource::KeyPackage(lifetime) = &self.leaf_node.leaf_node_source {
            Ok(lifetime.not_after)
        } else {
            Err(MlsError::InvalidLeafNodeSource)
        }
    }
}

impl<'a> Signable<'a> for KeyPackage {
    const SIGN_LABEL: &'static str = "KeyPackageTBS";

    type SigningContext = ();

    fn signature(&self) -> &[u8] {
        &self.signature
    }

    fn signable_content(
        &self,
        _context: &Self::SigningContext,
    ) -> Result<Vec<u8>, mls_rs_codec::Error> {
        KeyPackageData {
            version: self.version,
            cipher_suite: self.cipher_suite,
            hpke_init_key: &self.hpke_init_key,
            leaf_node: &self.leaf_node,
            extensions: &self.extensions,
        }
        .mls_encode_to_vec()
    }

    fn write_signature(&mut self, signature: Vec<u8>) {
        self.signature = signature
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    use super::*;
    use crate::{
        crypto::test_utils::test_cipher_suite_provider,
        group::framing::MlsMessagePayload,
        identity::basic::BasicIdentityProvider,
        identity::test_utils::get_test_signing_identity,
        tree_kem::{leaf_node::test_utils::get_test_capabilities, Lifetime},
        MlsMessage,
    };

    use mls_rs_core::crypto::SignatureSecretKey;

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn test_key_package(
        protocol_version: ProtocolVersion,
        cipher_suite: CipherSuite,
        id: &str,
    ) -> KeyPackage {
        test_key_package_with_signer(protocol_version, cipher_suite, id)
            .await
            .0
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn test_key_package_with_signer(
        protocol_version: ProtocolVersion,
        cipher_suite: CipherSuite,
        id: &str,
    ) -> (KeyPackage, SignatureSecretKey) {
        let (signing_identity, secret_key) =
            get_test_signing_identity(cipher_suite, id.as_bytes()).await;

        let generator = KeyPackageGenerator {
            protocol_version,
            cipher_suite_provider: &test_cipher_suite_provider(cipher_suite),
            signing_identity: &signing_identity,
            signing_key: &secret_key,
            identity_provider: &BasicIdentityProvider,
        };

        let key_package = generator
            .generate(
                Lifetime::years(1).unwrap(),
                get_test_capabilities(),
                ExtensionList::default(),
                ExtensionList::default(),
            )
            .await
            .unwrap()
            .key_package;

        (key_package, secret_key)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn test_key_package_message(
        protocol_version: ProtocolVersion,
        cipher_suite: CipherSuite,
        id: &str,
    ) -> MlsMessage {
        MlsMessage::new(
            protocol_version,
            MlsMessagePayload::KeyPackage(
                test_key_package(protocol_version, cipher_suite, id).await,
            ),
        )
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        client::test_utils::{TEST_CIPHER_SUITE, TEST_PROTOCOL_VERSION},
        crypto::test_utils::{test_cipher_suite_provider, try_test_cipher_suite_provider},
    };

    use super::{test_utils::test_key_package, *};
    use alloc::format;
    use assert_matches::assert_matches;

    #[derive(serde::Deserialize, serde::Serialize)]
    struct TestCase {
        cipher_suite: u16,
        #[serde(with = "hex::serde")]
        input: Vec<u8>,
        #[serde(with = "hex::serde")]
        output: Vec<u8>,
    }

    impl TestCase {
        #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
        #[cfg_attr(coverage_nightly, coverage(off))]
        async fn generate() -> Vec<TestCase> {
            let mut test_cases = Vec::new();

            for (i, (protocol_version, cipher_suite)) in ProtocolVersion::all()
                .flat_map(|p| CipherSuite::all().map(move |cs| (p, cs)))
                .enumerate()
            {
                let pkg =
                    test_key_package(protocol_version, cipher_suite, &format!("alice{i}")).await;

                let pkg_ref = pkg
                    .to_reference(&test_cipher_suite_provider(cipher_suite))
                    .await
                    .unwrap();

                let case = TestCase {
                    cipher_suite: cipher_suite.into(),
                    input: pkg.mls_encode_to_vec().unwrap(),
                    output: pkg_ref.to_vec(),
                };

                test_cases.push(case);
            }

            test_cases
        }
    }

    #[cfg(mls_build_async)]
    async fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(key_package_ref, TestCase::generate().await)
    }

    #[cfg(not(mls_build_async))]
    fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(key_package_ref, TestCase::generate())
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_key_package_ref() {
        let cases = load_test_cases().await;

        for one_case in cases {
            let Some(provider) = try_test_cipher_suite_provider(one_case.cipher_suite) else {
                continue;
            };

            let key_package = KeyPackage::mls_decode(&mut one_case.input.as_slice()).unwrap();

            let key_package_ref = key_package.to_reference(&provider).await.unwrap();

            let expected_out = KeyPackageRef::from(one_case.output);
            assert_eq!(expected_out, key_package_ref);
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn key_package_ref_fails_invalid_cipher_suite() {
        let key_package = test_key_package(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, "test").await;

        for another_cipher_suite in CipherSuite::all().filter(|cs| cs != &TEST_CIPHER_SUITE) {
            if let Some(cs) = try_test_cipher_suite_provider(*another_cipher_suite) {
                let res = key_package.to_reference(&cs).await;

                assert_matches!(res, Err(MlsError::CipherSuiteMismatch));
            }
        }
    }
}
