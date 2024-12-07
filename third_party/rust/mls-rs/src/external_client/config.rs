// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_core::identity::IdentityProvider;

use crate::{
    crypto::SignaturePublicKey,
    extension::ExtensionType,
    group::{mls_rules::MlsRules, proposal::ProposalType},
    identity::CredentialType,
    protocol_version::ProtocolVersion,
    tree_kem::Capabilities,
    CryptoProvider,
};

pub trait ExternalClientConfig: Send + Sync + Clone {
    type IdentityProvider: IdentityProvider + Clone;
    type MlsRules: MlsRules + Clone;
    type CryptoProvider: CryptoProvider;

    fn supported_extensions(&self) -> Vec<ExtensionType>;
    fn supported_custom_proposals(&self) -> Vec<ProposalType>;
    fn supported_protocol_versions(&self) -> Vec<ProtocolVersion>;
    fn identity_provider(&self) -> Self::IdentityProvider;
    fn crypto_provider(&self) -> Self::CryptoProvider;
    fn external_signing_key(&self, external_key_id: &[u8]) -> Option<SignaturePublicKey>;

    fn mls_rules(&self) -> Self::MlsRules;

    fn cache_proposals(&self) -> bool;

    fn max_epoch_jitter(&self) -> Option<u64> {
        None
    }

    fn capabilities(&self) -> Capabilities {
        Capabilities {
            protocol_versions: self.supported_protocol_versions(),
            cipher_suites: self.crypto_provider().supported_cipher_suites(),
            extensions: self.supported_extensions(),
            proposals: self.supported_custom_proposals(),
            credentials: self.supported_credentials(),
        }
    }

    fn version_supported(&self, version: ProtocolVersion) -> bool {
        self.supported_protocol_versions().contains(&version)
    }

    fn supported_credentials(&self) -> Vec<CredentialType> {
        self.identity_provider().supported_types()
    }
}
