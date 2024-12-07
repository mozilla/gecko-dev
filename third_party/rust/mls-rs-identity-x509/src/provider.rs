// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{util::credential_to_chain, CertificateChain, X509IdentityError};
use alloc::vec;
use alloc::vec::Vec;
use mls_rs_core::{
    crypto::SignaturePublicKey,
    error::IntoAnyError,
    extension::ExtensionList,
    identity::{CredentialType, IdentityProvider},
    time::MlsTime,
};

#[cfg(not(feature = "std"))]
use alloc::boxed::Box;

#[cfg(all(test, feature = "std"))]
use mockall::automock;

#[cfg_attr(all(test, feature = "std"), automock(type Error = crate::test_utils::TestError;))]
/// X.509 certificate unique identity trait.
pub trait X509IdentityExtractor {
    type Error: IntoAnyError;

    /// Produce a unique identity value to represent the entity controlling a
    /// certificate credential within an MLS group.
    fn identity(&self, certificate_chain: &CertificateChain) -> Result<Vec<u8>, Self::Error>;

    /// Determine if `successor` is controlled by the same entity as
    /// `predecessor`.
    fn valid_successor(
        &self,
        predecessor: &CertificateChain,
        successor: &CertificateChain,
    ) -> Result<bool, Self::Error>;
}

#[cfg_attr(all(test, feature = "std"), automock(type Error = crate::test_utils::TestError;))]
/// X.509 certificate validation trait.
pub trait X509CredentialValidator {
    type Error: IntoAnyError;

    /// Validate a certificate chain.
    ///
    /// If `timestamp` is set to `None` then expiration checks should be skipped.
    fn validate_chain(
        &self,
        chain: &CertificateChain,
        timestamp: Option<MlsTime>,
    ) -> Result<SignaturePublicKey, Self::Error>;
}

#[derive(Clone, Debug)]
#[non_exhaustive]
/// A customizable generic X.509 certificate identity provider.
///
/// This provider forwards its individual [`IdentityProvider`]
/// behavior to its generic sub-components.
///
/// Only X509 credentials are supported by this provider.
pub struct X509IdentityProvider<IE, V> {
    pub identity_extractor: IE,
    pub validator: V,
}

impl<IE, V> X509IdentityProvider<IE, V>
where
    IE: X509IdentityExtractor,
    V: X509CredentialValidator,
{
    /// Create a new identity provider.
    pub fn new(identity_extractor: IE, validator: V) -> Self {
        Self {
            identity_extractor,
            validator,
        }
    }

    /// Determine if a certificate is valid based on the behavior of the
    /// underlying validator provided.
    pub fn validate(
        &self,
        signing_identity: &mls_rs_core::identity::SigningIdentity,
        timestamp: Option<mls_rs_core::time::MlsTime>,
    ) -> Result<(), X509IdentityError> {
        let chain = credential_to_chain(&signing_identity.credential)?;

        let leaf_public_key = self
            .validator
            .validate_chain(&chain, timestamp)
            .map_err(|e| X509IdentityError::X509ValidationError(e.into_any_error()))?;

        if leaf_public_key != signing_identity.signature_key {
            return Err(X509IdentityError::SignatureKeyMismatch);
        }

        Ok(())
    }

    /// Produce a unique identity value to represent the entity controlling a
    /// certificate credential within an MLS group.
    pub fn identity(
        &self,
        signing_id: &mls_rs_core::identity::SigningIdentity,
    ) -> Result<Vec<u8>, X509IdentityError> {
        self.identity_extractor
            .identity(&credential_to_chain(&signing_id.credential)?)
            .map_err(|e| X509IdentityError::IdentityExtractorError(e.into_any_error()))
    }

    /// Determine if `successor` is controlled by the same entity as
    /// `predecessor` based on the behavior of the underlying identity
    /// extractor provided.
    pub fn valid_successor(
        &self,
        predecessor: &mls_rs_core::identity::SigningIdentity,
        successor: &mls_rs_core::identity::SigningIdentity,
    ) -> Result<bool, X509IdentityError> {
        self.identity_extractor
            .valid_successor(
                &credential_to_chain(&predecessor.credential)?,
                &credential_to_chain(&successor.credential)?,
            )
            .map_err(|e| X509IdentityError::IdentityExtractorError(e.into_any_error()))
    }

    /// Supported credential types.
    ///
    /// Only [`CredentialType::X509`] is supported.
    pub fn supported_types(&self) -> Vec<mls_rs_core::identity::CredentialType> {
        vec![CredentialType::X509]
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(mls_build_async, maybe_async::must_be_async)]
impl<IE, V> IdentityProvider for X509IdentityProvider<IE, V>
where
    IE: X509IdentityExtractor + Send + Sync,
    V: X509CredentialValidator + Send + Sync,
{
    type Error = X509IdentityError;

    async fn validate_member(
        &self,
        signing_identity: &mls_rs_core::identity::SigningIdentity,
        timestamp: Option<MlsTime>,
        _extensions: Option<&ExtensionList>,
    ) -> Result<(), Self::Error> {
        self.validate(signing_identity, timestamp)
    }

    async fn validate_external_sender(
        &self,
        signing_identity: &mls_rs_core::identity::SigningIdentity,
        timestamp: Option<MlsTime>,
        _extensions: Option<&ExtensionList>,
    ) -> Result<(), Self::Error> {
        self.validate(signing_identity, timestamp)
    }

    async fn identity(
        &self,
        signing_id: &mls_rs_core::identity::SigningIdentity,
        _extensions: &ExtensionList,
    ) -> Result<Vec<u8>, Self::Error> {
        self.identity(signing_id)
    }

    async fn valid_successor(
        &self,
        predecessor: &mls_rs_core::identity::SigningIdentity,
        successor: &mls_rs_core::identity::SigningIdentity,
        _extensions: &ExtensionList,
    ) -> Result<bool, Self::Error> {
        self.valid_successor(predecessor, successor)
    }

    fn supported_types(&self) -> Vec<CredentialType> {
        self.supported_types()
    }
}

#[cfg(all(test, feature = "std"))]
mod tests {
    use mls_rs_core::{crypto::SignaturePublicKey, identity::CredentialType, time::MlsTime};

    use crate::{
        test_utils::{
            test_certificate_chain, test_signing_identity, test_signing_identity_with_chain,
            TestError,
        },
        MockX509CredentialValidator, MockX509IdentityExtractor, X509IdentityError,
        X509IdentityProvider,
    };

    use alloc::vec;

    use assert_matches::assert_matches;

    #[cfg(target_arch = "wasm32")]
    use wasm_bindgen_test::wasm_bindgen_test as test;

    fn test_setup<F>(
        mut mock_setup: F,
    ) -> X509IdentityProvider<MockX509IdentityExtractor, MockX509CredentialValidator>
    where
        F: FnMut(&mut MockX509IdentityExtractor, &mut MockX509CredentialValidator),
    {
        let mut identity_extractor = MockX509IdentityExtractor::new();
        let mut validator = MockX509CredentialValidator::new();

        mock_setup(&mut identity_extractor, &mut validator);

        X509IdentityProvider::new(identity_extractor, validator)
    }

    #[test]
    fn test_supported_types() {
        let test_provider = test_setup(|_, _| ());

        assert_eq!(
            test_provider.supported_types(),
            vec![CredentialType::new(2)]
        )
    }

    #[test]
    fn test_successful_validation() {
        let chain = test_certificate_chain();

        let test_signing_identity = test_signing_identity_with_chain(chain.clone());

        let test_timestamp = MlsTime::now();

        let test_provider = test_setup(|_, validator| {
            let validation_result = test_signing_identity.signature_key.clone();

            validator
                .expect_validate_chain()
                .once()
                .with(
                    mockall::predicate::eq(chain.clone()),
                    mockall::predicate::eq(Some(test_timestamp)),
                )
                .return_once_st(|_, _| Ok(validation_result));
        });

        test_provider
            .validate(&test_signing_identity, Some(test_timestamp))
            .unwrap();
    }

    #[test]
    fn test_signing_identity_key_mismatch() {
        let test_signing_identity = test_signing_identity();

        let test_provider = test_setup(|_, validator| {
            let validation_result = SignaturePublicKey::from(vec![42u8; 32]);

            validator
                .expect_validate_chain()
                .return_once_st(|_, _| Ok(validation_result));
        });

        assert_matches!(
            test_provider.validate(&test_signing_identity, None),
            Err(X509IdentityError::SignatureKeyMismatch)
        );
    }

    #[test]
    fn test_failing_validation() {
        let test_provider = test_setup(|_, validator| {
            validator
                .expect_validate_chain()
                .return_once_st(|_, _| Err(TestError));
        });

        assert_matches!(
            test_provider.validate(&test_signing_identity(), None),
            Err(X509IdentityError::X509ValidationError(_))
        )
    }
}
