// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_core::identity::Credential;

use crate::{CertificateChain, X509IdentityError};

pub(crate) fn credential_to_chain(
    credential: &Credential,
) -> Result<CertificateChain, X509IdentityError> {
    credential
        .as_x509()
        .ok_or_else(|| X509IdentityError::UnsupportedCredentialType(credential.credential_type()))
        .cloned()
}
