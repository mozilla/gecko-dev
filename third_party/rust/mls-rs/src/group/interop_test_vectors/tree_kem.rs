// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{
    client::test_utils::TEST_PROTOCOL_VERSION,
    crypto::test_utils::try_test_cipher_suite_provider,
    group::{
        confirmation_tag::ConfirmationTag, framing::Content, message_processor::MessageProcessor,
        message_signature::AuthenticatedContent, test_utils::GroupWithoutKeySchedule, Commit,
        GroupContext, PathSecret, Sender,
    },
    identity::basic::BasicIdentityProvider,
    tree_kem::{
        node::{LeafIndex, NodeVec},
        TreeKemPrivate, TreeKemPublic, UpdatePath,
    },
    WireFormat,
};
use alloc::vec;
use alloc::vec::Vec;
use mls_rs_codec::MlsDecode;
use mls_rs_core::{crypto::CipherSuiteProvider, extension::ExtensionList};

#[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
struct TreeKemTestCase {
    pub cipher_suite: u16,

    #[serde(with = "hex::serde")]
    pub group_id: Vec<u8>,
    epoch: u64,
    #[serde(with = "hex::serde")]
    confirmed_transcript_hash: Vec<u8>,
    #[serde(with = "hex::serde")]
    ratchet_tree: Vec<u8>,

    leaves_private: Vec<TestLeafPrivate>,
    update_paths: Vec<TestUpdatePath>,
}

#[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
struct TestLeafPrivate {
    index: u32,
    #[serde(with = "hex::serde")]
    encryption_priv: Vec<u8>,
    #[serde(with = "hex::serde")]
    signature_priv: Vec<u8>,
    path_secrets: Vec<TestPathSecretPrivate>,
}

#[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
struct TestPathSecretPrivate {
    node: u32,
    #[serde(with = "hex::serde")]
    path_secret: Vec<u8>,
}

#[derive(serde::Serialize, serde::Deserialize, Debug, Default, Clone)]
struct TestUpdatePath {
    sender: u32,
    #[serde(with = "hex::serde")]
    update_path: Vec<u8>,
    #[serde(with = "hex::serde")]
    tree_hash_after: Vec<u8>,
    #[serde(with = "hex::serde")]
    commit_secret: Vec<u8>,
}

#[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
async fn tree_kem() {
    // The test vector can be found here https://github.com/mlswg/mls-implementations/blob/main/test-vectors/treekem.json

    let test_cases: Vec<TreeKemTestCase> =
        load_test_case_json!(interop_tree_kem, Vec::<TreeKemTestCase>::new());

    for test_case in test_cases {
        let Some(cs) = try_test_cipher_suite_provider(test_case.cipher_suite) else {
            continue;
        };

        // Import the public ratchet tree
        let nodes = NodeVec::mls_decode(&mut &*test_case.ratchet_tree).unwrap();

        let mut tree =
            TreeKemPublic::import_node_data(nodes, &BasicIdentityProvider, &Default::default())
                .await
                .unwrap();

        // Construct GroupContext
        let group_context = GroupContext {
            protocol_version: TEST_PROTOCOL_VERSION,
            cipher_suite: cs.cipher_suite(),
            group_id: test_case.group_id,
            epoch: test_case.epoch,
            tree_hash: tree.tree_hash(&cs).await.unwrap(),
            confirmed_transcript_hash: test_case.confirmed_transcript_hash.into(),
            extensions: ExtensionList::new(),
        };

        for leaf in test_case.leaves_private.iter() {
            // Construct the private ratchet tree
            let mut tree_private = TreeKemPrivate::new(LeafIndex(leaf.index));

            // Set and validate HPKE keys on direct path
            let path = tree.nodes.direct_copath(tree_private.self_index);

            tree_private.secret_keys = Vec::new();

            for dp in path {
                let dp = dp.path;

                let secret = leaf
                    .path_secrets
                    .iter()
                    .find_map(|s| (s.node == dp).then_some(s.path_secret.clone()));

                let private_key = if let Some(secret) = secret {
                    let (secret_key, public_key) = PathSecret::from(secret)
                        .to_hpke_key_pair(&cs)
                        .await
                        .unwrap();

                    let tree_public = &tree.nodes.borrow_as_parent(dp).unwrap().public_key;
                    assert_eq!(&public_key, tree_public);

                    Some(secret_key)
                } else {
                    None
                };

                tree_private.secret_keys.push(private_key);
            }

            // Set HPKE key for leaf
            tree_private
                .secret_keys
                .insert(0, Some(leaf.encryption_priv.clone().into()));

            let paths = test_case
                .update_paths
                .iter()
                .filter(|path| path.sender != leaf.index);

            for update_path in paths {
                let mut group = GroupWithoutKeySchedule::new(cs.cipher_suite()).await;
                group.state.context = group_context.clone();
                group.state.public_tree = tree.clone();
                group.private_tree = tree_private.clone();

                let path = UpdatePath::mls_decode(&mut &*update_path.update_path).unwrap();

                let commit = Commit {
                    proposals: vec![],
                    path: Some(path),
                };

                let mut auth_content = AuthenticatedContent::new(
                    &group_context,
                    Sender::Member(update_path.sender),
                    Content::Commit(alloc::boxed::Box::new(commit)),
                    vec![],
                    WireFormat::PublicMessage,
                );

                auth_content.auth.confirmation_tag = Some(ConfirmationTag::empty(&cs).await);

                // Hack not to increment epoch
                group.state.context.epoch -= 1;

                group.process_commit(auth_content, None).await.unwrap();

                // Check that we got the expected commit secret and correctly merged the update path.
                // This implies that we computed the path secrets correctly.
                let commit_secret = group.secrets.unwrap().1;

                assert_eq!(&*commit_secret, &update_path.commit_secret);

                let new_tree = &mut group.provisional_public_state.unwrap().public_tree;
                let new_tree_hash = new_tree.tree_hash(&cs).await.unwrap();

                assert_eq!(&new_tree_hash, &update_path.tree_hash_after);
            }
        }
    }
}
