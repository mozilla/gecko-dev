// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use super::leaf_node::{LeafNode, LeafNodeSigningContext, LeafNodeSource};
use crate::client::MlsError;
use crate::CipherSuiteProvider;
use crate::{signer::Signable, time::MlsTime};
use mls_rs_core::{error::IntoAnyError, extension::ExtensionList, identity::IdentityProvider};

use crate::extension::RequiredCapabilitiesExt;

#[cfg(feature = "by_ref_proposal")]
use crate::extension::ExternalSendersExt;

pub enum ValidationContext<'a> {
    Add(Option<MlsTime>),
    Update((&'a [u8], u32, Option<MlsTime>)),
    Commit((&'a [u8], u32, Option<MlsTime>)),
}

impl<'a> ValidationContext<'a> {
    fn signing_context(&self) -> LeafNodeSigningContext {
        match *self {
            ValidationContext::Add(_) => Default::default(),
            ValidationContext::Update((group_id, leaf_index, _)) => (group_id, leaf_index).into(),
            ValidationContext::Commit((group_id, leaf_index, _)) => (group_id, leaf_index).into(),
        }
    }

    fn generation_time(&self) -> Option<MlsTime> {
        match *self {
            ValidationContext::Add(t) => t,
            ValidationContext::Update((_, _, t)) => t,
            ValidationContext::Commit((_, _, t)) => t,
        }
    }
}

#[derive(Clone, Debug)]
pub struct LeafNodeValidator<'a, C, CP>
where
    C: IdentityProvider,
    CP: CipherSuiteProvider,
{
    cipher_suite_provider: &'a CP,
    identity_provider: &'a C,
    group_context_extensions: Option<&'a ExtensionList>,
}

impl<'a, C: IdentityProvider, CP: CipherSuiteProvider> LeafNodeValidator<'a, C, CP> {
    pub fn new(
        cipher_suite_provider: &'a CP,
        identity_provider: &'a C,
        group_context_extensions: Option<&'a ExtensionList>,
    ) -> Self {
        Self {
            cipher_suite_provider,
            identity_provider,
            group_context_extensions,
        }
    }

    fn check_context(
        &self,
        leaf_node: &LeafNode,
        context: &ValidationContext,
    ) -> Result<(), MlsError> {
        // Context specific checks
        match context {
            ValidationContext::Add(time) => {
                // If the context is add, and we specified a time to check for lifetime, verify it
                if let LeafNodeSource::KeyPackage(lifetime) = &leaf_node.leaf_node_source {
                    if let Some(current_time) = time {
                        if !lifetime.within_lifetime(*current_time) {
                            return Err(MlsError::InvalidLifetime);
                        }
                    }
                } else {
                    // If the leaf_node_source is anything other than Add it is invalid
                    return Err(MlsError::InvalidLeafNodeSource);
                }
            }
            ValidationContext::Update(_) => {
                // If the leaf_node_source is anything other than Update it is invalid
                if !matches!(leaf_node.leaf_node_source, LeafNodeSource::Update) {
                    return Err(MlsError::InvalidLeafNodeSource);
                }
            }
            ValidationContext::Commit(_) => {
                // If the leaf_node_source is anything other than Commit it is invalid
                if !matches!(leaf_node.leaf_node_source, LeafNodeSource::Commit(_)) {
                    return Err(MlsError::InvalidLeafNodeSource);
                }
            }
        }

        Ok(())
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn revalidate(
        &self,
        leaf_node: &LeafNode,
        group_id: &[u8],
        leaf_index: u32,
    ) -> Result<(), MlsError> {
        let context = match leaf_node.leaf_node_source {
            LeafNodeSource::KeyPackage(_) => ValidationContext::Add(None),
            LeafNodeSource::Update => ValidationContext::Update((group_id, leaf_index, None)),
            LeafNodeSource::Commit(_) => ValidationContext::Commit((group_id, leaf_index, None)),
        };

        self.check_if_valid(leaf_node, context).await
    }

    pub fn validate_required_capabilities(&self, leaf_node: &LeafNode) -> Result<(), MlsError> {
        let Some(required_capabilities) = self
            .group_context_extensions
            .and_then(|exts| exts.get_as::<RequiredCapabilitiesExt>().transpose())
            .transpose()?
        else {
            return Ok(());
        };

        for extension in &required_capabilities.extensions {
            if !leaf_node.capabilities.extensions.contains(extension) {
                return Err(MlsError::RequiredExtensionNotFound(*extension));
            }
        }

        for proposal in &required_capabilities.proposals {
            if !leaf_node.capabilities.proposals.contains(proposal) {
                return Err(MlsError::RequiredProposalNotFound(*proposal));
            }
        }

        for credential in &required_capabilities.credentials {
            if !leaf_node.capabilities.credentials.contains(credential) {
                return Err(MlsError::RequiredCredentialNotFound(*credential));
            }
        }

        Ok(())
    }

    #[cfg(feature = "by_ref_proposal")]
    pub fn validate_external_senders_ext_credentials(
        &self,
        leaf_node: &LeafNode,
    ) -> Result<(), MlsError> {
        let Some(ext) = self
            .group_context_extensions
            .and_then(|exts| exts.get_as::<ExternalSendersExt>().transpose())
            .transpose()?
        else {
            return Ok(());
        };

        ext.allowed_senders.iter().try_for_each(|sender| {
            let cred_type = sender.credential.credential_type();
            leaf_node
                .capabilities
                .credentials
                .contains(&cred_type)
                .then_some(())
                .ok_or(MlsError::RequiredCredentialNotFound(cred_type))
        })
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn check_if_valid(
        &self,
        leaf_node: &LeafNode,
        context: ValidationContext<'_>,
    ) -> Result<(), MlsError> {
        // Check that we are validating within the proper context
        self.check_context(leaf_node, &context)?;

        // Verify the credential
        self.identity_provider
            .validate_member(
                &leaf_node.signing_identity,
                context.generation_time(),
                self.group_context_extensions,
            )
            .await
            .map_err(|e| MlsError::IdentityProviderError(e.into_any_error()))?;

        // Verify that the credential signed the leaf node
        leaf_node
            .verify(
                self.cipher_suite_provider,
                &leaf_node.signing_identity.signature_key,
                &context.signing_context(),
            )
            .await?;

        // If required capabilities are specified, verify the leaf node meets the requirements
        self.validate_required_capabilities(leaf_node)?;

        // If there are extensions, make sure they are referenced in the capabilities field
        for one_ext in &*leaf_node.extensions {
            if !leaf_node
                .capabilities
                .extensions
                .contains(&one_ext.extension_type)
            {
                return Err(MlsError::ExtensionNotInCapabilities(one_ext.extension_type));
            }
        }

        // Verify that group extensions are supported by the leaf
        self.group_context_extensions
            .into_iter()
            .flat_map(|exts| &**exts)
            .map(|ext| ext.extension_type)
            .find(|ext_type| {
                !ext_type.is_default() && !leaf_node.capabilities.extensions.contains(ext_type)
            })
            .map(MlsError::UnsupportedGroupExtension)
            .map_or(Ok(()), Err)?;

        #[cfg(feature = "by_ref_proposal")]
        self.validate_external_senders_ext_credentials(leaf_node)?;

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use crate::crypto::test_utils::try_test_cipher_suite_provider;
    use crate::extension::MlsExtension;
    use alloc::vec;
    use assert_matches::assert_matches;
    #[cfg(feature = "std")]
    use core::time::Duration;
    use mls_rs_core::crypto::CipherSuite;
    use mls_rs_core::group::ProposalType;

    use super::*;

    use crate::client::test_utils::TEST_CIPHER_SUITE;
    use crate::crypto::test_utils::test_cipher_suite_provider;
    use crate::crypto::test_utils::TestCryptoProvider;
    use crate::crypto::SignatureSecretKey;
    use crate::extension::test_utils::TestExtension;
    use crate::group::test_utils::random_bytes;
    use crate::identity::basic::BasicCredential;
    use crate::identity::basic::BasicIdentityProvider;
    use crate::identity::test_utils::get_test_signing_identity;
    use crate::tree_kem::leaf_node::test_utils::*;
    use crate::tree_kem::leaf_node_validator::test_utils::FailureIdentityProvider;
    use crate::tree_kem::Capabilities;
    use crate::ExtensionList;

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn get_test_add_node() -> (LeafNode, SignatureSecretKey) {
        let (signing_identity, secret) = get_test_signing_identity(TEST_CIPHER_SUITE, b"foo").await;

        let (leaf_node, _) =
            get_test_node(TEST_CIPHER_SUITE, signing_identity, &secret, None, None).await;

        (leaf_node, secret)
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_basic_add_validation() {
        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        let (leaf_node, _) = get_test_add_node().await;

        let test_validator =
            LeafNodeValidator::new(&cipher_suite_provider, &BasicIdentityProvider, None);

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Add(None))
            .await;

        assert_matches!(res, Ok(_));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_failed_validation() {
        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);
        let (leaf_node, _) = get_test_add_node().await;

        let fail_test_validator =
            LeafNodeValidator::new(&cipher_suite_provider, &FailureIdentityProvider, None);

        let res = fail_test_validator
            .check_if_valid(&leaf_node, ValidationContext::Add(None))
            .await;

        assert_matches!(res, Err(MlsError::IdentityProviderError(_)));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_basic_update_validation() {
        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);
        let group_id = b"group_id";

        let (mut leaf_node, secret) = get_test_add_node().await;

        leaf_node
            .update(
                &cipher_suite_provider,
                group_id,
                0,
                // TODO remove identity from input
                default_properties(),
                None,
                &secret,
            )
            .await
            .unwrap();

        let test_validator =
            LeafNodeValidator::new(&cipher_suite_provider, &BasicIdentityProvider, None);

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Update((group_id, 0, None)))
            .await;

        assert_matches!(res, Ok(_));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_basic_commit_validation() {
        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);
        let group_id = b"group_id";

        let (mut leaf_node, secret) = get_test_add_node().await;

        leaf_node.leaf_node_source = LeafNodeSource::Commit(hex!("f00d").into());

        leaf_node
            .commit(
                &cipher_suite_provider,
                group_id,
                0,
                default_properties(),
                None,
                &secret,
            )
            .await
            .unwrap();

        let test_validator =
            LeafNodeValidator::new(&cipher_suite_provider, &BasicIdentityProvider, None);

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Commit((group_id, 0, None)))
            .await;

        assert_matches!(res, Ok(_));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_incorrect_context() {
        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        let test_validator =
            LeafNodeValidator::new(&cipher_suite_provider, &BasicIdentityProvider, None);

        let (mut leaf_node, secret) = get_test_add_node().await;

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Update((b"foo", 0, None)))
            .await;

        assert_matches!(res, Err(MlsError::InvalidLeafNodeSource));

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Commit((b"foo", 0, None)))
            .await;

        assert_matches!(res, Err(MlsError::InvalidLeafNodeSource));

        leaf_node
            .update(
                &cipher_suite_provider,
                b"foo",
                0,
                default_properties(),
                None,
                &secret,
            )
            .await
            .unwrap();

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Add(None))
            .await;

        assert_matches!(res, Err(MlsError::InvalidLeafNodeSource));

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Commit((b"foo", 0, None)))
            .await;

        assert_matches!(res, Err(MlsError::InvalidLeafNodeSource));

        leaf_node.leaf_node_source = LeafNodeSource::Commit(hex!("f00d").into());

        leaf_node
            .commit(
                &cipher_suite_provider,
                b"foo",
                0,
                default_properties(),
                None,
                &secret,
            )
            .await
            .unwrap();

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Add(None))
            .await;

        assert_matches!(res, Err(MlsError::InvalidLeafNodeSource));

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Update((b"foo", 0, None)))
            .await;

        assert_matches!(res, Err(MlsError::InvalidLeafNodeSource));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_bad_signature() {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let cipher_suite_provider = test_cipher_suite_provider(cipher_suite);

            let (signing_identity, secret) = get_test_signing_identity(cipher_suite, b"foo").await;

            let (mut leaf_node, _) =
                get_test_node(cipher_suite, signing_identity, &secret, None, None).await;

            leaf_node.signature = random_bytes(leaf_node.signature.len());

            let test_validator =
                LeafNodeValidator::new(&cipher_suite_provider, &BasicIdentityProvider, None);

            let res = test_validator
                .check_if_valid(&leaf_node, ValidationContext::Add(None))
                .await;

            assert_matches!(res, Err(MlsError::InvalidSignature));
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_capabilities_mismatch() {
        let (signing_identity, secret) = get_test_signing_identity(TEST_CIPHER_SUITE, b"foo").await;

        let mut extensions = ExtensionList::new();

        extensions.set_from(TestExtension::from(0)).unwrap();

        let capabilities = Capabilities {
            credentials: vec![BasicCredential::credential_type()],
            ..Default::default()
        };

        let (leaf_node, _) = get_test_node(
            TEST_CIPHER_SUITE,
            signing_identity,
            &secret,
            Some(capabilities),
            Some(extensions),
        )
        .await;

        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        let test_validator =
            LeafNodeValidator::new(&cipher_suite_provider, &BasicIdentityProvider, None);

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Add(None))
            .await;

        assert_matches!(res,
            Err(MlsError::ExtensionNotInCapabilities(ext)) if ext == 42.into());
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_cipher_suite_mismatch() {
        for another_cipher_suite in CipherSuite::all().filter(|cs| cs != &TEST_CIPHER_SUITE) {
            if let Some(cs) = try_test_cipher_suite_provider(*another_cipher_suite) {
                let (leaf_node, _) = get_test_add_node().await;

                let test_validator = LeafNodeValidator::new(&cs, &BasicIdentityProvider, None);

                let res = test_validator
                    .check_if_valid(&leaf_node, ValidationContext::Add(None))
                    .await;

                assert_matches!(res, Err(MlsError::InvalidSignature));
            }
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_required_extension() {
        let required_capabilities = RequiredCapabilitiesExt {
            extensions: vec![43.into()],
            ..Default::default()
        };

        let (leaf_node, _) = get_test_add_node().await;

        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        let group_context_extensions =
            core::iter::once(required_capabilities.into_extension().unwrap()).collect();

        let test_validator = LeafNodeValidator::new(
            &cipher_suite_provider,
            &BasicIdentityProvider,
            Some(&group_context_extensions),
        );

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Add(None))
            .await;

        assert_matches!(
            res,
            Err(MlsError::RequiredExtensionNotFound(v)) if v == 43.into()
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_required_proposal() {
        let required_capabilities = RequiredCapabilitiesExt {
            proposals: vec![42.into()],
            ..Default::default()
        };

        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        let (leaf_node, _) = get_test_add_node().await;

        let group_context_extensions =
            core::iter::once(required_capabilities.into_extension().unwrap()).collect();

        let test_validator = LeafNodeValidator::new(
            &cipher_suite_provider,
            &BasicIdentityProvider,
            Some(&group_context_extensions),
        );

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Add(None))
            .await;

        assert_matches!(
            res,
            Err(MlsError::RequiredProposalNotFound(p)) if p == ProposalType::new(42)
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_required_credential() {
        let required_capabilities = RequiredCapabilitiesExt {
            credentials: vec![0.into()],
            ..Default::default()
        };

        let (leaf_node, _) = get_test_add_node().await;

        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        let group_context_extensions =
            core::iter::once(required_capabilities.into_extension().unwrap()).collect();

        let test_validator = LeafNodeValidator::new(
            &cipher_suite_provider,
            &BasicIdentityProvider,
            Some(&group_context_extensions),
        );

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Add(None))
            .await;

        assert_matches!(res,
            Err(MlsError::RequiredCredentialNotFound(ext)) if ext == 0.into()
        );
    }

    #[cfg(feature = "std")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_add_lifetime() {
        let (leaf_node, _) = get_test_add_node().await;

        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);

        let test_validator =
            LeafNodeValidator::new(&cipher_suite_provider, &BasicIdentityProvider, None);

        let good_lifetime = MlsTime::now();

        let over_one_year = good_lifetime.seconds_since_epoch() + (86400 * 366);

        let bad_lifetime = MlsTime::from_duration_since_epoch(Duration::from_secs(over_one_year));

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Add(Some(good_lifetime)))
            .await;

        assert_matches!(res, Ok(()));

        let res = test_validator
            .check_if_valid(&leaf_node, ValidationContext::Add(Some(bad_lifetime)))
            .await;

        assert_matches!(res, Err(MlsError::InvalidLifetime));
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    use alloc::vec;
    use alloc::{boxed::Box, vec::Vec};
    use mls_rs_codec::MlsEncode;
    use mls_rs_core::{
        error::IntoAnyError,
        extension::ExtensionList,
        identity::{BasicCredential, IdentityProvider},
    };

    use crate::{identity::SigningIdentity, time::MlsTime};

    #[derive(Clone, Debug, Default)]
    pub struct FailureIdentityProvider;

    #[cfg(feature = "by_ref_proposal")]
    impl FailureIdentityProvider {
        pub fn new() -> Self {
            Self
        }
    }

    #[derive(Debug)]
    #[cfg_attr(feature = "std", derive(thiserror::Error))]
    #[cfg_attr(feature = "std", error("test error"))]
    pub struct TestFailureError;

    impl IntoAnyError for TestFailureError {
        #[cfg(feature = "std")]
        fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
            Ok(self.into())
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    #[cfg_attr(mls_build_async, maybe_async::must_be_async)]
    impl IdentityProvider for FailureIdentityProvider {
        type Error = TestFailureError;

        async fn validate_member(
            &self,
            _signing_identity: &SigningIdentity,
            _timestamp: Option<MlsTime>,
            _extensions: Option<&ExtensionList>,
        ) -> Result<(), Self::Error> {
            Err(TestFailureError)
        }

        async fn validate_external_sender(
            &self,
            _signing_identity: &SigningIdentity,
            _timestamp: Option<MlsTime>,
            _extensions: Option<&ExtensionList>,
        ) -> Result<(), Self::Error> {
            Err(TestFailureError)
        }

        #[cfg_attr(coverage_nightly, coverage(off))]
        async fn identity(
            &self,
            signing_id: &SigningIdentity,
            _extensions: &ExtensionList,
        ) -> Result<Vec<u8>, Self::Error> {
            Ok(signing_id.credential.mls_encode_to_vec().unwrap())
        }

        #[cfg_attr(coverage_nightly, coverage(off))]
        async fn valid_successor(
            &self,
            _predecessor: &SigningIdentity,
            _successor: &SigningIdentity,
            _extensions: &ExtensionList,
        ) -> Result<bool, Self::Error> {
            Err(TestFailureError)
        }

        #[cfg_attr(coverage_nightly, coverage(off))]
        fn supported_types(&self) -> Vec<crate::identity::CredentialType> {
            vec![BasicCredential::credential_type()]
        }
    }
}
