// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// Basic credential identity provider.
pub mod basic;

/// X.509 certificate identity provider.
#[cfg(feature = "x509")]
pub mod x509 {
    pub use mls_rs_identity_x509::*;
}

pub use mls_rs_core::identity::{
    Credential, CredentialType, CustomCredential, MlsCredential, SigningIdentity,
};

pub use mls_rs_core::group::RosterUpdate;

#[cfg(test)]
pub(crate) mod test_utils {
    use alloc::boxed::Box;
    use alloc::vec;
    use alloc::vec::Vec;
    use mls_rs_core::{
        crypto::{CipherSuite, CipherSuiteProvider, SignatureSecretKey},
        error::IntoAnyError,
        extension::ExtensionList,
        identity::{Credential, CredentialType, IdentityProvider, SigningIdentity},
        time::MlsTime,
    };

    use crate::crypto::test_utils::test_cipher_suite_provider;

    use super::basic::{BasicCredential, BasicIdentityProvider};

    #[derive(Debug)]
    #[cfg_attr(feature = "std", derive(thiserror::Error))]
    #[cfg_attr(
        feature = "std",
        error("expected basic or custom credential type 42 found: {0:?}")
    )]
    pub struct BasicWithCustomProviderError(CredentialType);

    impl IntoAnyError for BasicWithCustomProviderError {
        #[cfg(feature = "std")]
        fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
            Ok(self.into())
        }
    }

    #[derive(Debug, Clone)]
    pub struct BasicWithCustomProvider {
        pub(crate) basic: BasicIdentityProvider,
        pub(crate) allow_any_custom: bool,
        supported_cred_types: Vec<CredentialType>,
    }

    impl BasicWithCustomProvider {
        pub const CUSTOM_CREDENTIAL_TYPE: u16 = 42;

        pub fn new(basic: BasicIdentityProvider) -> BasicWithCustomProvider {
            BasicWithCustomProvider {
                basic,
                allow_any_custom: false,
                supported_cred_types: vec![
                    CredentialType::BASIC,
                    Self::CUSTOM_CREDENTIAL_TYPE.into(),
                ],
            }
        }

        pub fn with_credential_type(mut self, cred_type: CredentialType) -> Self {
            self.supported_cred_types.push(cred_type);
            self
        }

        #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
        async fn resolve_custom_identity(
            &self,
            signing_id: &SigningIdentity,
        ) -> Result<Vec<u8>, BasicWithCustomProviderError> {
            self.basic
                .identity(signing_id, &Default::default())
                .await
                .or_else(|_| {
                    signing_id
                        .credential
                        .as_custom()
                        .map(|c| {
                            if c.credential_type
                                == CredentialType::from(Self::CUSTOM_CREDENTIAL_TYPE)
                                || self.allow_any_custom
                            {
                                Ok(c.data.to_vec())
                            } else {
                                Err(BasicWithCustomProviderError(c.credential_type))
                            }
                        })
                        .transpose()?
                        .ok_or_else(|| {
                            BasicWithCustomProviderError(signing_id.credential.credential_type())
                        })
                })
        }
    }

    impl Default for BasicWithCustomProvider {
        fn default() -> Self {
            Self::new(BasicIdentityProvider::new())
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    #[cfg_attr(mls_build_async, maybe_async::must_be_async)]
    impl IdentityProvider for BasicWithCustomProvider {
        type Error = BasicWithCustomProviderError;

        async fn validate_member(
            &self,
            _signing_identity: &SigningIdentity,
            _timestamp: Option<MlsTime>,
            _extensions: Option<&ExtensionList>,
        ) -> Result<(), Self::Error> {
            //TODO: Is it actually beneficial to check the key, or does that already happen elsewhere before
            //this point?
            Ok(())
        }

        async fn validate_external_sender(
            &self,
            _signing_identity: &SigningIdentity,
            _timestamp: Option<MlsTime>,
            _extensions: Option<&ExtensionList>,
        ) -> Result<(), Self::Error> {
            //TODO: Is it actually beneficial to check the key, or does that already happen elsewhere before
            //this point?
            Ok(())
        }

        async fn identity(
            &self,
            signing_id: &SigningIdentity,
            _extensions: &ExtensionList,
        ) -> Result<Vec<u8>, Self::Error> {
            self.resolve_custom_identity(signing_id).await
        }

        async fn valid_successor(
            &self,
            predecessor: &SigningIdentity,
            successor: &SigningIdentity,
            _extensions: &ExtensionList,
        ) -> Result<bool, Self::Error> {
            let predecessor = self.resolve_custom_identity(predecessor).await?;
            let successor = self.resolve_custom_identity(successor).await?;

            Ok(predecessor == successor)
        }

        fn supported_types(&self) -> Vec<CredentialType> {
            self.supported_cred_types.clone()
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn get_test_signing_identity(
        cipher_suite: CipherSuite,
        identity: &[u8],
    ) -> (SigningIdentity, SignatureSecretKey) {
        let provider = test_cipher_suite_provider(cipher_suite);
        let (secret_key, public_key) = provider.signature_key_generate().await.unwrap();

        let basic = get_test_basic_credential(identity.to_vec());

        (SigningIdentity::new(basic, public_key), secret_key)
    }

    pub fn get_test_basic_credential(identity: Vec<u8>) -> Credential {
        BasicCredential::new(identity).into_credential()
    }
}
