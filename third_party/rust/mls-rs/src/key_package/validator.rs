// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_core::{crypto::CipherSuiteProvider, protocol_version::ProtocolVersion};

use crate::{client::MlsError, signer::Signable, KeyPackage};

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn validate_key_package_properties<CSP: CipherSuiteProvider>(
    package: &KeyPackage,
    version: ProtocolVersion,
    cs: &CSP,
) -> Result<(), MlsError> {
    package
        .verify(cs, &package.leaf_node.signing_identity.signature_key, &())
        .await?;

    // Verify that the protocol version matches
    if package.version != version {
        return Err(MlsError::ProtocolVersionMismatch);
    }

    // Verify that the cipher suite matches
    if package.cipher_suite != cs.cipher_suite() {
        return Err(MlsError::CipherSuiteMismatch);
    }

    // Verify that the public init key is a valid format for this cipher suite
    cs.kem_public_key_validate(&package.hpke_init_key)
        .map_err(|_| MlsError::InvalidInitKey)?;

    // Verify that the init key and the leaf node public key are different
    if package.hpke_init_key.as_ref() == package.leaf_node.public_key.as_ref() {
        return Err(MlsError::InitLeafKeyEquality);
    }

    Ok(())
}
