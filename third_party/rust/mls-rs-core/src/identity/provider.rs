// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{error::IntoAnyError, extension::ExtensionList, time::MlsTime};
#[cfg(mls_build_async)]
use alloc::boxed::Box;
use alloc::vec::Vec;

use super::{CredentialType, SigningIdentity};

/// Identity system that can be used to validate a
/// [`SigningIdentity`](mls-rs-core::identity::SigningIdentity)
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(mls_build_async, maybe_async::must_be_async)]
pub trait IdentityProvider: Send + Sync {
    /// Error type that this provider returns on internal failure.
    type Error: IntoAnyError;

    /// Determine if `signing_identity` is valid for a group member.
    ///
    /// A `timestamp` value can optionally be supplied to aid with validation
    /// of a [`Credential`](mls-rs-core::identity::Credential) that requires
    /// time based context. For example, X.509 certificates can become expired.
    async fn validate_member(
        &self,
        signing_identity: &SigningIdentity,
        timestamp: Option<MlsTime>,
        extensions: Option<&ExtensionList>,
    ) -> Result<(), Self::Error>;

    /// Determine if `signing_identity` is valid for an external sender in
    /// the ExternalSendersExtension stored in the group context.
    ///
    /// A `timestamp` value can optionally be supplied to aid with validation
    /// of a [`Credential`](mls-rs-core::identity::Credential) that requires
    /// time based context. For example, X.509 certificates can become expired.
    async fn validate_external_sender(
        &self,
        signing_identity: &SigningIdentity,
        timestamp: Option<MlsTime>,
        extensions: Option<&ExtensionList>,
    ) -> Result<(), Self::Error>;

    /// A unique identifier for `signing_identity`.
    ///
    /// The MLS protocol requires that each member of a group has a unique
    /// set of identifiers according to the application.
    async fn identity(
        &self,
        signing_identity: &SigningIdentity,
        extensions: &ExtensionList,
    ) -> Result<Vec<u8>, Self::Error>;

    /// Determines if `successor` can remove `predecessor` as part of an external commit.
    ///
    /// The MLS protocol allows for removal of an existing member when adding a
    /// new member via external commit. This function determines if a removal
    /// should be allowed by providing the target member to be removed as
    /// `predecessor` and the new member as `successor`.
    async fn valid_successor(
        &self,
        predecessor: &SigningIdentity,
        successor: &SigningIdentity,
        extensions: &ExtensionList,
    ) -> Result<bool, Self::Error>;

    /// Credential types that are supported by this provider.
    fn supported_types(&self) -> Vec<CredentialType>;
}
