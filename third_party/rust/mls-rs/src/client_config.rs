// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{
    extension::ExtensionType,
    group::{mls_rules::MlsRules, proposal::ProposalType},
    identity::CredentialType,
    protocol_version::ProtocolVersion,
    tree_kem::{leaf_node::ConfigProperties, Capabilities, Lifetime},
    ExtensionList,
};
use alloc::vec::Vec;
use mls_rs_core::{
    crypto::CryptoProvider, group::GroupStateStorage, identity::IdentityProvider,
    key_package::KeyPackageStorage, psk::PreSharedKeyStorage,
};

pub trait ClientConfig: Send + Sync + Clone {
    type KeyPackageRepository: KeyPackageStorage + Clone;
    type PskStore: PreSharedKeyStorage + Clone;
    type GroupStateStorage: GroupStateStorage + Clone;
    type IdentityProvider: IdentityProvider + Clone;
    type MlsRules: MlsRules + Clone;
    type CryptoProvider: CryptoProvider + Clone;

    fn supported_extensions(&self) -> Vec<ExtensionType>;
    fn supported_custom_proposals(&self) -> Vec<ProposalType>;
    fn supported_protocol_versions(&self) -> Vec<ProtocolVersion>;

    fn key_package_repo(&self) -> Self::KeyPackageRepository;

    fn mls_rules(&self) -> Self::MlsRules;

    fn secret_store(&self) -> Self::PskStore;
    fn group_state_storage(&self) -> Self::GroupStateStorage;
    fn identity_provider(&self) -> Self::IdentityProvider;
    fn crypto_provider(&self) -> Self::CryptoProvider;

    fn key_package_extensions(&self) -> ExtensionList;
    fn leaf_node_extensions(&self) -> ExtensionList;
    fn lifetime(&self) -> Lifetime;

    fn capabilities(&self) -> Capabilities {
        Capabilities {
            protocol_versions: self.supported_protocol_versions(),
            cipher_suites: self.crypto_provider().supported_cipher_suites(),
            extensions: self.supported_extensions(),
            proposals: self.supported_custom_proposals(),
            credentials: self.supported_credential_types(),
        }
    }

    fn version_supported(&self, version: ProtocolVersion) -> bool {
        self.supported_protocol_versions().contains(&version)
    }

    fn supported_credential_types(&self) -> Vec<CredentialType> {
        self.identity_provider().supported_types()
    }

    fn leaf_properties(&self) -> ConfigProperties {
        ConfigProperties {
            capabilities: self.capabilities(),
            extensions: self.leaf_node_extensions(),
        }
    }
}
