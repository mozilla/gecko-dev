// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec;
use alloc::vec::Vec;
use mls_rs_codec::{MlsDecode, MlsEncode};
use mls_rs_core::crypto::{CipherSuite, CipherSuiteProvider, SignaturePublicKey};

use crate::{
    client::test_utils::{TestClientConfig, TEST_PROTOCOL_VERSION},
    crypto::test_utils::{test_cipher_suite_provider, try_test_cipher_suite_provider},
    group::{
        confirmation_tag::ConfirmationTag,
        epoch::EpochSecrets,
        framing::{Content, WireFormat},
        message_processor::{EventOrContent, MessageProcessor},
        mls_rules::EncryptionOptions,
        padding::PaddingMode,
        proposal::{Proposal, RemoveProposal},
        secret_tree::test_utils::get_test_tree,
        test_utils::{random_bytes, test_group_custom_config},
        AuthenticatedContent, Commit, Group, GroupContext, MlsMessage, Sender,
    },
    mls_rules::DefaultMlsRules,
    test_utils::is_edwards,
    tree_kem::{leaf_node::test_utils::get_basic_test_node, node::LeafIndex},
};

const FRAMING_N_LEAVES: u32 = 2;

#[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
struct FramingTestCase {
    #[serde(flatten)]
    pub context: InteropGroupContext,

    #[serde(with = "hex::serde")]
    pub signature_priv: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub signature_pub: Vec<u8>,

    #[serde(with = "hex::serde")]
    pub encryption_secret: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub sender_data_secret: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub membership_key: Vec<u8>,

    #[serde(with = "hex::serde")]
    pub proposal: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub proposal_priv: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub proposal_pub: Vec<u8>,

    #[serde(with = "hex::serde")]
    pub commit: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub commit_priv: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub commit_pub: Vec<u8>,

    #[serde(with = "hex::serde")]
    pub application: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub application_priv: Vec<u8>,
}

impl FramingTestCase {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    #[cfg_attr(coverage_nightly, coverage(off))]
    async fn random<P: CipherSuiteProvider>(cs: &P) -> Self {
        let mut context = InteropGroupContext::random(cs);
        context.cipher_suite = cs.cipher_suite().into();

        let (mut signature_priv, signature_pub) = cs.signature_key_generate().await.unwrap();

        if is_edwards(*cs.cipher_suite()) {
            signature_priv = signature_priv[0..signature_priv.len() / 2].to_vec().into();
        }

        Self {
            context,
            signature_priv: signature_priv.to_vec(),
            signature_pub: signature_pub.to_vec(),
            encryption_secret: random_bytes(cs.kdf_extract_size()),
            sender_data_secret: random_bytes(cs.kdf_extract_size()),
            membership_key: random_bytes(cs.kdf_extract_size()),
            ..Default::default()
        }
    }
}

#[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
pub struct InteropGroupContext {
    pub cipher_suite: u16,
    #[serde(with = "hex::serde")]
    pub group_id: Vec<u8>,
    pub epoch: u64,
    #[serde(with = "hex::serde")]
    pub tree_hash: Vec<u8>,
    #[serde(with = "hex::serde")]
    pub confirmed_transcript_hash: Vec<u8>,
}

impl InteropGroupContext {
    #[cfg_attr(coverage_nightly, coverage(off))]
    fn random<P: CipherSuiteProvider>(cs: &P) -> Self {
        Self {
            cipher_suite: cs.cipher_suite().into(),
            group_id: random_bytes(cs.kdf_extract_size()),
            epoch: 0x121212,
            tree_hash: random_bytes(cs.kdf_extract_size()),
            confirmed_transcript_hash: random_bytes(cs.kdf_extract_size()),
        }
    }
}

impl From<InteropGroupContext> for GroupContext {
    #[cfg_attr(coverage_nightly, coverage(off))]
    fn from(ctx: InteropGroupContext) -> Self {
        Self {
            cipher_suite: ctx.cipher_suite.into(),
            protocol_version: TEST_PROTOCOL_VERSION,
            group_id: ctx.group_id,
            epoch: ctx.epoch,
            tree_hash: ctx.tree_hash,
            confirmed_transcript_hash: ctx.confirmed_transcript_hash.into(),
            extensions: vec![].into(),
        }
    }
}

// The test vector can be found here:
// https://github.com/mlswg/mls-implementations/blob/main/test-vectors/message-protection.json
#[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
async fn framing_proposal() {
    #[cfg(not(mls_build_async))]
    let test_cases: Vec<FramingTestCase> =
        load_test_case_json!(framing, generate_framing_test_vector());

    #[cfg(mls_build_async)]
    let test_cases: Vec<FramingTestCase> =
        load_test_case_json!(framing, generate_framing_test_vector().await);

    for test_case in test_cases.into_iter() {
        let Some(cs) = try_test_cipher_suite_provider(test_case.context.cipher_suite) else {
            continue;
        };

        let to_check = vec![
            test_case.proposal_priv.clone(),
            test_case.proposal_pub.clone(),
        ];

        // Wasm uses incompatible signature secret key format
        #[cfg(not(target_arch = "wasm32"))]
        let mut to_check = to_check;

        #[cfg(not(target_arch = "wasm32"))]
        for enable_encryption in [true, false] {
            let proposal = Proposal::mls_decode(&mut &*test_case.proposal).unwrap();

            let built = make_group(&test_case, true, enable_encryption, &cs)
                .await
                .proposal_message(proposal, vec![])
                .await
                .unwrap()
                .mls_encode_to_vec()
                .unwrap();

            to_check.push(built);
        }

        let proposal = Proposal::mls_decode(&mut &*test_case.proposal).unwrap();

        for message in to_check {
            match process_message(&test_case, &message, &cs).await {
                Content::Proposal(p) => assert_eq!(p.as_ref(), &proposal),
                _ => panic!("received value not proposal"),
            };
        }
    }
}

// The test vector can be found here:
// https://github.com/mlswg/mls-implementations/blob/main/test-vectors/message-protection.json
// Wasm uses incompatible signature secret key format
#[cfg(not(target_arch = "wasm32"))]
#[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
async fn framing_application() {
    #[cfg(not(mls_build_async))]
    let test_cases: Vec<FramingTestCase> =
        load_test_case_json!(framing, generate_framing_test_vector());

    #[cfg(mls_build_async)]
    let test_cases: Vec<FramingTestCase> =
        load_test_case_json!(framing, generate_framing_test_vector().await);

    for test_case in test_cases.into_iter() {
        let Some(cs) = try_test_cipher_suite_provider(test_case.context.cipher_suite) else {
            continue;
        };

        let built_priv = make_group(&test_case, true, true, &cs)
            .await
            .encrypt_application_message(&test_case.application, vec![])
            .await
            .unwrap()
            .mls_encode_to_vec()
            .unwrap();

        for message in [&test_case.application_priv, &built_priv] {
            match process_message(&test_case, message, &cs).await {
                Content::Application(data) => assert_eq!(data.as_ref(), &test_case.application),
                _ => panic!("decrypted value not application data"),
            };
        }
    }
}

// The test vector can be found here:
// https://github.com/mlswg/mls-implementations/blob/main/test-vectors/message-protection.json
#[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
async fn framing_commit() {
    #[cfg(not(mls_build_async))]
    let test_cases: Vec<FramingTestCase> =
        load_test_case_json!(framing, generate_framing_test_vector());

    #[cfg(mls_build_async)]
    let test_cases: Vec<FramingTestCase> =
        load_test_case_json!(framing, generate_framing_test_vector().await);

    for test_case in test_cases.into_iter() {
        let Some(cs) = try_test_cipher_suite_provider(test_case.context.cipher_suite) else {
            continue;
        };

        let commit = Commit::mls_decode(&mut &*test_case.commit).unwrap();

        let to_check = vec![test_case.commit_priv.clone(), test_case.commit_pub.clone()];

        // Wasm uses incompatible signature secret key format
        #[cfg(not(target_arch = "wasm32"))]
        let to_check = {
            let mut to_check = to_check;

            let mut signature_priv = test_case.signature_priv.clone();

            if is_edwards(test_case.context.cipher_suite) {
                signature_priv.extend(test_case.signature_pub.iter());
            }

            let mut auth_content = AuthenticatedContent::new_signed(
                &cs,
                &test_case.context.clone().into(),
                Sender::Member(1),
                Content::Commit(alloc::boxed::Box::new(commit.clone())),
                &signature_priv.into(),
                WireFormat::PublicMessage,
                vec![],
            )
            .await
            .unwrap();

            auth_content.auth.confirmation_tag = Some(ConfirmationTag::empty(&cs).await);

            for enable_encryption in [true, false] {
                let built = make_group(&test_case, true, enable_encryption, &cs)
                    .await
                    .format_for_wire(auth_content.clone())
                    .await
                    .unwrap()
                    .mls_encode_to_vec()
                    .unwrap();

                to_check.push(built);
            }

            to_check
        };

        for message in to_check {
            match process_message(&test_case, &message, &cs).await {
                Content::Commit(c) => assert_eq!(&*c, &commit),
                _ => panic!("received value not commit"),
            };
        }
        let commit = Commit::mls_decode(&mut &*test_case.commit).unwrap();

        match process_message(&test_case, &test_case.commit_priv.clone(), &cs).await {
            Content::Commit(c) => assert_eq!(&*c, &commit),
            _ => panic!("received value not commit"),
        };
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(coverage_nightly, coverage(off))]
async fn generate_framing_test_vector() -> Vec<FramingTestCase> {
    let mut test_vector = vec![];

    for cs in CipherSuite::all() {
        let cs = test_cipher_suite_provider(cs);

        let mut test_case = FramingTestCase::random(&cs).await;

        // Generate private application message
        test_case.application = cs.random_bytes_vec(42).unwrap();

        let application_priv = make_group(&test_case, true, true, &cs)
            .await
            .encrypt_application_message(&test_case.application, vec![])
            .await
            .unwrap();

        test_case.application_priv = application_priv.mls_encode_to_vec().unwrap();

        // Generate private and public proposal message
        let proposal = Proposal::Remove(RemoveProposal {
            to_remove: LeafIndex(2),
        });

        test_case.proposal = proposal.mls_encode_to_vec().unwrap();

        let mut group = make_group(&test_case, true, false, &cs).await;
        let proposal_pub = group.proposal_message(proposal.clone(), vec![]).await;
        test_case.proposal_pub = proposal_pub.unwrap().mls_encode_to_vec().unwrap();

        let mut group = make_group(&test_case, true, true, &cs).await;
        let proposal_priv = group.proposal_message(proposal, vec![]).await.unwrap();
        test_case.proposal_priv = proposal_priv.mls_encode_to_vec().unwrap();

        // Generate private and public commit message
        let commit = Commit {
            proposals: vec![],
            path: None,
        };

        test_case.commit = commit.mls_encode_to_vec().unwrap();

        let mut auth_content = AuthenticatedContent::new_signed(
            &cs,
            group.context(),
            Sender::Member(1),
            Content::Commit(alloc::boxed::Box::new(commit.clone())),
            &group.signer,
            WireFormat::PublicMessage,
            vec![],
        )
        .await
        .unwrap();

        auth_content.auth.confirmation_tag = Some(ConfirmationTag::empty(&cs).await);

        let mut group = make_group(&test_case, true, false, &cs).await;
        let commit_pub = group.format_for_wire(auth_content.clone()).await.unwrap();
        test_case.commit_pub = commit_pub.mls_encode_to_vec().unwrap();

        let mut auth_content = AuthenticatedContent::new_signed(
            &cs,
            group.context(),
            Sender::Member(1),
            Content::Commit(alloc::boxed::Box::new(commit)),
            &group.signer,
            WireFormat::PrivateMessage,
            vec![],
        )
        .await
        .unwrap();

        auth_content.auth.confirmation_tag = Some(ConfirmationTag::empty(&cs).await);

        let mut group = make_group(&test_case, true, true, &cs).await;
        let commit_priv = group.format_for_wire(auth_content.clone()).await.unwrap();
        test_case.commit_priv = commit_priv.mls_encode_to_vec().unwrap();

        test_vector.push(test_case);
    }

    test_vector
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn make_group<P: CipherSuiteProvider>(
    test_case: &FramingTestCase,
    for_send: bool,
    control_encryption_enabled: bool,
    cs: &P,
) -> Group<TestClientConfig> {
    let mut group =
        test_group_custom_config(
            TEST_PROTOCOL_VERSION,
            test_case.context.cipher_suite.into(),
            |b| {
                b.mls_rules(DefaultMlsRules::default().with_encryption_options(
                    EncryptionOptions::new(control_encryption_enabled, PaddingMode::None),
                ))
            },
        )
        .await
        .group;

    // Add a leaf for the sender. It will get index 1.
    let mut leaf = get_basic_test_node(cs.cipher_suite(), "leaf").await;

    leaf.signing_identity.signature_key = SignaturePublicKey::from(test_case.signature_pub.clone());

    group
        .state
        .public_tree
        .add_leaves(vec![leaf], &group.config.0.identity_provider, cs)
        .await
        .unwrap();

    // Convince the group that their index is 1 if they send or 0 if they receive.
    group.private_tree.self_index = LeafIndex(if for_send { 1 } else { 0 });

    // Convince the group that their signing key is the one from the test case
    let mut signature_priv = test_case.signature_priv.clone();

    if is_edwards(test_case.context.cipher_suite) {
        signature_priv.extend(test_case.signature_pub.iter());
    }

    group.signer = signature_priv.into();

    // Set the group context and secrets
    let context = GroupContext::from(test_case.context.clone());
    let secret_tree = get_test_tree(test_case.encryption_secret.clone(), FRAMING_N_LEAVES);

    let secrets = EpochSecrets {
        secret_tree,
        resumption_secret: vec![0_u8; cs.kdf_extract_size()].into(),
        sender_data_secret: test_case.sender_data_secret.clone().into(),
    };

    group.epoch_secrets = secrets;
    group.state.context = context;
    let membership_key = test_case.membership_key.clone();
    group.key_schedule.set_membership_key(membership_key);

    group
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn process_message<P: CipherSuiteProvider>(
    test_case: &FramingTestCase,
    message: &[u8],
    cs: &P,
) -> Content {
    // Enabling encryption doesn't matter for processing
    let mut group = make_group(test_case, false, true, cs).await;
    let message = MlsMessage::mls_decode(&mut &*message).unwrap();
    let evt_or_cont = group.get_event_from_incoming_message(message);

    match evt_or_cont.await.unwrap() {
        EventOrContent::Content(content) => content.content.content,
        EventOrContent::Event(_) => panic!("expected content, got event"),
    }
}
