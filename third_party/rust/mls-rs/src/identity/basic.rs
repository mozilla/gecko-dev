// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{identity::CredentialType, identity::SigningIdentity, time::MlsTime};
use alloc::vec;
use alloc::vec::Vec;
pub use mls_rs_core::identity::BasicCredential;
use mls_rs_core::{error::IntoAnyError, extension::ExtensionList, identity::IdentityProvider};

#[derive(Debug)]
#[cfg_attr(feature = "std", derive(thiserror::Error))]
#[cfg_attr(feature = "std", error("unsupported credential type found: {0:?}"))]
/// Error returned in the event that a non-basic
/// credential is passed to a [`BasicIdentityProvider`].
pub struct BasicIdentityProviderError(CredentialType);

impl IntoAnyError for BasicIdentityProviderError {
    #[cfg(feature = "std")]
    fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
        Ok(self.into())
    }
}

impl BasicIdentityProviderError {
    pub fn credential_type(&self) -> CredentialType {
        self.0
    }
}

#[derive(Clone, Debug, Default)]
/// An always-valid identity provider that works with [`BasicCredential`].
///
/// # Warning
///
/// This provider always returns `true` for `validate` as long as the
/// [`SigningIdentity`] used contains a [`BasicCredential`]. It is only
/// recommended to use this provider for testing purposes.
pub struct BasicIdentityProvider;

impl BasicIdentityProvider {
    pub fn new() -> Self {
        Self
    }
}

fn resolve_basic_identity(
    signing_id: &SigningIdentity,
) -> Result<&BasicCredential, BasicIdentityProviderError> {
    signing_id
        .credential
        .as_basic()
        .ok_or_else(|| BasicIdentityProviderError(signing_id.credential.credential_type()))
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(mls_build_async, maybe_async::must_be_async)]
impl IdentityProvider for BasicIdentityProvider {
    type Error = BasicIdentityProviderError;

    async fn validate_member(
        &self,
        signing_identity: &SigningIdentity,
        _timestamp: Option<MlsTime>,
        _extensions: Option<&ExtensionList>,
    ) -> Result<(), Self::Error> {
        resolve_basic_identity(signing_identity).map(|_| ())
    }

    async fn validate_external_sender(
        &self,
        signing_identity: &SigningIdentity,
        _timestamp: Option<MlsTime>,
        _extensions: Option<&ExtensionList>,
    ) -> Result<(), Self::Error> {
        resolve_basic_identity(signing_identity).map(|_| ())
    }

    async fn identity(
        &self,
        signing_identity: &SigningIdentity,
        _extensions: &ExtensionList,
    ) -> Result<Vec<u8>, Self::Error> {
        resolve_basic_identity(signing_identity).map(|b| b.identifier.to_vec())
    }

    async fn valid_successor(
        &self,
        predecessor: &SigningIdentity,
        successor: &SigningIdentity,
        _extensions: &ExtensionList,
    ) -> Result<bool, Self::Error> {
        Ok(resolve_basic_identity(predecessor)? == resolve_basic_identity(successor)?)
    }

    fn supported_types(&self) -> Vec<CredentialType> {
        vec![BasicCredential::credential_type()]
    }
}
