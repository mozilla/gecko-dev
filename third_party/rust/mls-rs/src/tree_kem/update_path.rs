// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::{vec, vec::Vec};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::{error::IntoAnyError, identity::IdentityProvider};

use super::{
    leaf_node::LeafNode,
    leaf_node_validator::{LeafNodeValidator, ValidationContext},
    node::LeafIndex,
};
use crate::{
    client::MlsError,
    crypto::{CipherSuiteProvider, HpkeCiphertext, HpkePublicKey},
};
use crate::{group::message_processor::ProvisionalState, time::MlsTime};

#[derive(Clone, Debug, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct UpdatePathNode {
    pub public_key: HpkePublicKey,
    pub encrypted_path_secret: Vec<HpkeCiphertext>,
}

#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct UpdatePath {
    pub leaf_node: LeafNode,
    pub nodes: Vec<UpdatePathNode>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct ValidatedUpdatePath {
    pub leaf_node: LeafNode,
    pub nodes: Vec<Option<UpdatePathNode>>,
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(crate) async fn validate_update_path<C: IdentityProvider, CSP: CipherSuiteProvider>(
    identity_provider: &C,
    cipher_suite_provider: &CSP,
    path: UpdatePath,
    state: &ProvisionalState,
    sender: LeafIndex,
    commit_time: Option<MlsTime>,
) -> Result<ValidatedUpdatePath, MlsError> {
    let group_context_extensions = &state.group_context.extensions;

    let leaf_validator = LeafNodeValidator::new(
        cipher_suite_provider,
        identity_provider,
        Some(group_context_extensions),
    );

    leaf_validator
        .check_if_valid(
            &path.leaf_node,
            ValidationContext::Commit((&state.group_context.group_id, *sender, commit_time)),
        )
        .await?;

    let check_identity_eq = state.applied_proposals.external_initializations.is_empty();

    if check_identity_eq {
        let existing_leaf = state.public_tree.nodes.borrow_as_leaf(sender)?;
        let original_leaf_node = existing_leaf.clone();

        identity_provider
            .valid_successor(
                &original_leaf_node.signing_identity,
                &path.leaf_node.signing_identity,
                group_context_extensions,
            )
            .await
            .map_err(|e| MlsError::IdentityProviderError(e.into_any_error()))?
            .then_some(())
            .ok_or(MlsError::InvalidSuccessor)?;

        (existing_leaf.public_key != path.leaf_node.public_key)
            .then_some(())
            .ok_or(MlsError::SameHpkeKey(*sender))?;
    }

    // Unfilter the update path
    let filtered = state.public_tree.nodes.filtered(sender)?;
    let mut unfiltered_nodes = vec![];
    let mut i = 0;

    for n in path.nodes {
        while *filtered.get(i).ok_or(MlsError::WrongPathLen)? {
            unfiltered_nodes.push(None);
            i += 1;
        }

        unfiltered_nodes.push(Some(n));
        i += 1;
    }

    Ok(ValidatedUpdatePath {
        leaf_node: path.leaf_node,
        nodes: unfiltered_nodes,
    })
}

#[cfg(test)]
mod tests {
    use alloc::vec;
    use assert_matches::assert_matches;

    use crate::client::test_utils::TEST_CIPHER_SUITE;
    use crate::crypto::test_utils::test_cipher_suite_provider;
    use crate::crypto::HpkeCiphertext;
    use crate::group::message_processor::ProvisionalState;
    use crate::group::test_utils::{get_test_group_context, random_bytes, TEST_GROUP};
    use crate::identity::basic::BasicIdentityProvider;
    use crate::tree_kem::leaf_node::test_utils::default_properties;
    use crate::tree_kem::leaf_node::test_utils::get_basic_test_node_sig_key;
    use crate::tree_kem::leaf_node::LeafNodeSource;
    use crate::tree_kem::node::LeafIndex;
    use crate::tree_kem::parent_hash::ParentHash;
    use crate::tree_kem::test_utils::{get_test_leaf_nodes, get_test_tree};
    use crate::tree_kem::validate_update_path;

    use super::{UpdatePath, UpdatePathNode};
    use crate::{cipher_suite::CipherSuite, tree_kem::MlsError};

    use alloc::vec::Vec;

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_update_path(cipher_suite: CipherSuite, cred: &str) -> UpdatePath {
        let (mut leaf_node, _, signer) = get_basic_test_node_sig_key(cipher_suite, cred).await;

        leaf_node.leaf_node_source = LeafNodeSource::Commit(ParentHash::from(hex!("beef")));

        leaf_node
            .commit(
                &test_cipher_suite_provider(cipher_suite),
                TEST_GROUP,
                0,
                default_properties(),
                None,
                &signer,
            )
            .await
            .unwrap();

        let node = UpdatePathNode {
            public_key: random_bytes(32).into(),
            encrypted_path_secret: vec![HpkeCiphertext {
                kem_output: random_bytes(32),
                ciphertext: random_bytes(32),
            }],
        };

        UpdatePath {
            leaf_node,
            nodes: vec![node.clone(), node],
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_provisional_state(cipher_suite: CipherSuite) -> ProvisionalState {
        let mut tree = get_test_tree(cipher_suite).await.public;
        let leaf_nodes = get_test_leaf_nodes(cipher_suite).await;

        tree.add_leaves(
            leaf_nodes,
            &BasicIdentityProvider,
            &test_cipher_suite_provider(cipher_suite),
        )
        .await
        .unwrap();

        ProvisionalState {
            public_tree: tree,
            applied_proposals: Default::default(),
            group_context: get_test_group_context(1, cipher_suite).await,
            indexes_of_added_kpkgs: vec![],
            external_init_index: None,
            #[cfg(feature = "state_update")]
            unused_proposals: vec![],
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_valid_leaf_node() {
        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);
        let update_path = test_update_path(TEST_CIPHER_SUITE, "creator").await;

        let validated = validate_update_path(
            &BasicIdentityProvider,
            &cipher_suite_provider,
            update_path.clone(),
            &test_provisional_state(TEST_CIPHER_SUITE).await,
            LeafIndex(0),
            None,
        )
        .await
        .unwrap();

        let expected = update_path.nodes.into_iter().map(Some).collect::<Vec<_>>();

        assert_eq!(validated.nodes, expected);
        assert_eq!(validated.leaf_node, update_path.leaf_node);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_invalid_key_package() {
        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);
        let mut update_path = test_update_path(TEST_CIPHER_SUITE, "creator").await;
        update_path.leaf_node.signature = random_bytes(32);

        let validated = validate_update_path(
            &BasicIdentityProvider,
            &cipher_suite_provider,
            update_path,
            &test_provisional_state(TEST_CIPHER_SUITE).await,
            LeafIndex(0),
            None,
        )
        .await;

        assert_matches!(validated, Err(MlsError::InvalidSignature));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn validating_path_fails_with_different_identity() {
        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);
        let cipher_suite = TEST_CIPHER_SUITE;
        let update_path = test_update_path(cipher_suite, "foobar").await;

        let validated = validate_update_path(
            &BasicIdentityProvider,
            &cipher_suite_provider,
            update_path,
            &test_provisional_state(cipher_suite).await,
            LeafIndex(0),
            None,
        )
        .await;

        assert_matches!(validated, Err(MlsError::InvalidSuccessor));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn validating_path_fails_with_same_hpke_key() {
        let cipher_suite_provider = test_cipher_suite_provider(TEST_CIPHER_SUITE);
        let update_path = test_update_path(TEST_CIPHER_SUITE, "creator").await;
        let mut state = test_provisional_state(TEST_CIPHER_SUITE).await;

        state
            .public_tree
            .nodes
            .borrow_as_leaf_mut(LeafIndex(0))
            .unwrap()
            .public_key = update_path.leaf_node.public_key.clone();

        let validated = validate_update_path(
            &BasicIdentityProvider,
            &cipher_suite_provider,
            update_path,
            &state,
            LeafIndex(0),
            None,
        )
        .await;

        assert_matches!(validated, Err(MlsError::SameHpkeKey(_)));
    }
}
