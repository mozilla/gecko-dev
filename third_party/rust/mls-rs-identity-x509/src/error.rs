// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_core::{error::AnyError, identity::CredentialType};

#[derive(Debug)]
#[cfg_attr(feature = "std", derive(thiserror::Error))]
pub enum X509IdentityError {
    #[cfg_attr(feature = "std", error("unsupported credential type {0:?}"))]
    UnsupportedCredentialType(CredentialType),
    #[cfg_attr(
        feature = "std",
        error("signing identity public key does not match the leaf certificate")
    )]
    SignatureKeyMismatch,
    #[cfg_attr(feature = "std", error("unable to parse certificate chain data"))]
    InvalidCertificateChain,
    #[cfg_attr(feature = "std", error("invalid offset within certificate chain"))]
    InvalidOffset,
    #[cfg_attr(feature = "std", error("empty certificate chain"))]
    EmptyCertificateChain,
    #[cfg_attr(feature = "std", error(transparent))]
    CredentialEncodingError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    X509ReaderError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    IdentityExtractorError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    X509ValidationError(AnyError),
    #[cfg_attr(feature = "std", error(transparent))]
    IdentityWarningProviderError(AnyError),
}

impl mls_rs_core::error::IntoAnyError for X509IdentityError {
    #[cfg(feature = "std")]
    fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
        Ok(self.into())
    }
}
