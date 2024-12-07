// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#[cfg(all(feature = "benchmark_util", not(mls_build_async)))]
pub mod benchmarks;

#[cfg(all(feature = "fuzz_util", not(mls_build_async)))]
pub mod fuzz_tests;

use mls_rs_core::{
    crypto::{CipherSuite, CipherSuiteProvider, CryptoProvider},
    identity::{BasicCredential, Credential, SigningIdentity},
    protocol_version::ProtocolVersion,
    psk::ExternalPskId,
};

use crate::{
    client_builder::{ClientBuilder, MlsConfig},
    identity::basic::BasicIdentityProvider,
    mls_rules::{CommitOptions, DefaultMlsRules},
    tree_kem::Lifetime,
    Client, Group, MlsMessage,
};

#[cfg(feature = "private_message")]
use crate::group::{mls_rules::EncryptionOptions, padding::PaddingMode};

use alloc::{vec, vec::Vec};

#[cfg_attr(coverage_nightly, coverage(off))]
pub fn get_test_basic_credential(identity: Vec<u8>) -> Credential {
    BasicCredential::new(identity).into_credential()
}

pub const TEST_EXT_PSK_ID: &[u8] = b"external psk";

#[cfg_attr(coverage_nightly, coverage(off))]
pub fn make_test_ext_psk() -> Vec<u8> {
    b"secret psk key".to_vec()
}

pub fn is_edwards(cs: u16) -> bool {
    [
        CipherSuite::CURVE25519_AES128,
        CipherSuite::CURVE25519_CHACHA,
        CipherSuite::CURVE448_AES256,
        CipherSuite::CURVE448_CHACHA,
    ]
    .contains(&cs.into())
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(coverage_nightly, coverage(off))]
pub async fn generate_basic_client<C: CryptoProvider + Clone>(
    cipher_suite: CipherSuite,
    protocol_version: ProtocolVersion,
    id: usize,
    commit_options: Option<CommitOptions>,
    #[cfg(feature = "private_message")] encrypt_controls: bool,
    #[cfg(not(feature = "private_message"))] _encrypt_controls: bool,
    crypto: &C,
    lifetime: Option<Lifetime>,
) -> Client<impl MlsConfig> {
    let cs = crypto.cipher_suite_provider(cipher_suite).unwrap();

    let (secret_key, public_key) = cs.signature_key_generate().await.unwrap();
    let credential = get_test_basic_credential(alloc::format!("{id}").into_bytes());

    let identity = SigningIdentity::new(credential, public_key);

    let mls_rules =
        DefaultMlsRules::default().with_commit_options(commit_options.unwrap_or_default());

    #[cfg(feature = "private_message")]
    let mls_rules = if encrypt_controls {
        mls_rules.with_encryption_options(EncryptionOptions::new(true, PaddingMode::None))
    } else {
        mls_rules
    };

    let mut builder = ClientBuilder::new()
        .crypto_provider(crypto.clone())
        .identity_provider(BasicIdentityProvider::new())
        .mls_rules(mls_rules)
        .psk(
            ExternalPskId::new(TEST_EXT_PSK_ID.to_vec()),
            make_test_ext_psk().into(),
        )
        .used_protocol_version(protocol_version)
        .signing_identity(identity, secret_key, cipher_suite);

    if let Some(lifetime) = lifetime {
        builder = builder
            .key_package_lifetime(lifetime.not_after - lifetime.not_before)
            .key_package_not_before(lifetime.not_before);
    }

    builder.build()
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(coverage_nightly, coverage(off))]
pub async fn get_test_groups<C: CryptoProvider + Clone>(
    version: ProtocolVersion,
    cipher_suite: CipherSuite,
    num_participants: usize,
    commit_options: Option<CommitOptions>,
    encrypt_controls: bool,
    crypto: &C,
) -> Vec<Group<impl MlsConfig>> {
    // Create the group with Alice as the group initiator
    let creator = generate_basic_client(
        cipher_suite,
        version,
        0,
        commit_options,
        encrypt_controls,
        crypto,
        None,
    )
    .await;

    let mut creator_group = creator.create_group(Default::default()).await.unwrap();

    let mut receiver_clients = Vec::new();
    let mut commit_builder = creator_group.commit_builder();

    for i in 1..num_participants {
        let client = generate_basic_client(
            cipher_suite,
            version,
            i,
            commit_options,
            encrypt_controls,
            crypto,
            None,
        )
        .await;
        let kp = client.generate_key_package_message().await.unwrap();

        receiver_clients.push(client);
        commit_builder = commit_builder.add_member(kp.clone()).unwrap();
    }

    let welcome = commit_builder.build().await.unwrap().welcome_messages;

    creator_group.apply_pending_commit().await.unwrap();

    let tree_data = creator_group.export_tree().into_owned();

    let mut groups = vec![creator_group];

    for client in &receiver_clients {
        let (test_client, _info) = client
            .join_group(Some(tree_data.clone()), &welcome[0])
            .await
            .unwrap();

        groups.push(test_client);
    }

    groups
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(coverage_nightly, coverage(off))]
pub async fn all_process_message<C: MlsConfig>(
    groups: &mut [Group<C>],
    message: &MlsMessage,
    sender: usize,
    is_commit: bool,
) {
    for group in groups {
        if sender != group.current_member_index() as usize {
            group
                .process_incoming_message(message.clone())
                .await
                .unwrap();
        } else if is_commit {
            group.apply_pending_commit().await.unwrap();
        }
    }
}
