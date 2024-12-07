// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::vec::Vec;
use mls_rs_codec::{MlsDecode, MlsEncode};

use mls_rs_core::extension::ExtensionList;

use crate::{
    group::{
        framing::ContentType,
        proposal::{
            AddProposal, ExternalInit, PreSharedKeyProposal, ReInitProposal, RemoveProposal,
            UpdateProposal,
        },
        Commit, GroupSecrets, MlsMessage,
    },
    tree_kem::node::NodeVec,
};

#[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
struct TestCase {
    #[serde(with = "hex::serde")]
    mls_welcome: Vec<u8>,
    #[serde(with = "hex::serde")]
    mls_group_info: Vec<u8>,
    #[serde(with = "hex::serde")]
    mls_key_package: Vec<u8>,

    #[serde(with = "hex::serde")]
    ratchet_tree: Vec<u8>,
    #[serde(with = "hex::serde")]
    group_secrets: Vec<u8>,

    #[serde(with = "hex::serde")]
    add_proposal: Vec<u8>,
    #[serde(with = "hex::serde")]
    update_proposal: Vec<u8>,
    #[serde(with = "hex::serde")]
    remove_proposal: Vec<u8>,
    #[serde(with = "hex::serde")]
    pre_shared_key_proposal: Vec<u8>,
    #[serde(with = "hex::serde")]
    re_init_proposal: Vec<u8>,
    #[serde(with = "hex::serde")]
    external_init_proposal: Vec<u8>,
    #[serde(with = "hex::serde")]
    group_context_extensions_proposal: Vec<u8>,

    #[serde(with = "hex::serde")]
    commit: Vec<u8>,

    #[serde(with = "hex::serde")]
    public_message_application: Vec<u8>,
    #[serde(with = "hex::serde")]
    public_message_proposal: Vec<u8>,
    #[serde(with = "hex::serde")]
    public_message_commit: Vec<u8>,
    #[serde(with = "hex::serde")]
    private_message: Vec<u8>,
}

// The test vector can be found here:
// https://github.com/mlswg/mls-implementations/blob/main/test-vectors/messages.json
#[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
async fn serialization() {
    let test_cases: Vec<TestCase> = load_test_case_json!(serialization, Vec::<TestCase>::new());

    for test_case in test_cases.into_iter() {
        let message = MlsMessage::from_bytes(&test_case.mls_welcome).unwrap();
        message.clone().into_welcome().unwrap();
        assert_eq!(&message.to_bytes().unwrap(), &test_case.mls_welcome);

        let message = MlsMessage::from_bytes(&test_case.mls_group_info).unwrap();
        message.clone().into_group_info().unwrap();
        assert_eq!(&message.to_bytes().unwrap(), &test_case.mls_group_info);

        let message = MlsMessage::from_bytes(&test_case.mls_key_package).unwrap();
        message.clone().into_key_package().unwrap();
        assert_eq!(&message.to_bytes().unwrap(), &test_case.mls_key_package);

        let tree = NodeVec::mls_decode(&mut &*test_case.ratchet_tree).unwrap();

        assert_eq!(&tree.mls_encode_to_vec().unwrap(), &test_case.ratchet_tree);

        let secs = GroupSecrets::mls_decode(&mut &*test_case.group_secrets).unwrap();

        assert_eq!(&secs.mls_encode_to_vec().unwrap(), &test_case.group_secrets);

        let proposal = AddProposal::mls_decode(&mut &*test_case.add_proposal).unwrap();

        assert_eq!(
            &proposal.mls_encode_to_vec().unwrap(),
            &test_case.add_proposal
        );

        let proposal = UpdateProposal::mls_decode(&mut &*test_case.update_proposal).unwrap();

        assert_eq!(
            &proposal.mls_encode_to_vec().unwrap(),
            &test_case.update_proposal
        );

        let proposal = RemoveProposal::mls_decode(&mut &*test_case.remove_proposal).unwrap();

        assert_eq!(
            &proposal.mls_encode_to_vec().unwrap(),
            &test_case.remove_proposal
        );

        let proposal = ReInitProposal::mls_decode(&mut &*test_case.re_init_proposal).unwrap();

        assert_eq!(
            &proposal.mls_encode_to_vec().unwrap(),
            &test_case.re_init_proposal
        );

        let proposal =
            PreSharedKeyProposal::mls_decode(&mut &*test_case.pre_shared_key_proposal).unwrap();

        assert_eq!(
            &proposal.mls_encode_to_vec().unwrap(),
            &test_case.pre_shared_key_proposal
        );

        let proposal = ExternalInit::mls_decode(&mut &*test_case.external_init_proposal).unwrap();

        assert_eq!(
            &proposal.mls_encode_to_vec().unwrap(),
            &test_case.external_init_proposal
        );

        let proposal =
            ExtensionList::mls_decode(&mut &*test_case.group_context_extensions_proposal).unwrap();

        assert_eq!(
            &proposal.mls_encode_to_vec().unwrap(),
            &test_case.group_context_extensions_proposal
        );

        let commit = Commit::mls_decode(&mut &*test_case.commit).unwrap();

        assert_eq!(&commit.mls_encode_to_vec().unwrap(), &test_case.commit);

        let message = MlsMessage::from_bytes(&test_case.public_message_application).unwrap();
        let serialized = message.mls_encode_to_vec().unwrap();
        assert_eq!(&serialized, &test_case.public_message_application);
        let content_type = message.into_plaintext().unwrap().content.content_type();
        assert_eq!(content_type, ContentType::Application);

        let message = MlsMessage::from_bytes(&test_case.public_message_proposal).unwrap();
        let serialized = message.mls_encode_to_vec().unwrap();
        assert_eq!(&serialized, &test_case.public_message_proposal);
        let content_type = message.into_plaintext().unwrap().content.content_type();
        assert_eq!(content_type, ContentType::Proposal);

        let message = MlsMessage::from_bytes(&test_case.public_message_commit).unwrap();
        let serialized = message.mls_encode_to_vec().unwrap();
        assert_eq!(&serialized, &test_case.public_message_commit);
        let content_type = message.into_plaintext().unwrap().content.content_type();
        assert_eq!(content_type, ContentType::Commit);

        let message = MlsMessage::from_bytes(&test_case.private_message).unwrap();
        let serialized = message.mls_encode_to_vec().unwrap();
        assert_eq!(&serialized, &test_case.private_message);
        message.into_ciphertext().unwrap();
    }
}
