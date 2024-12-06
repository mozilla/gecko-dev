// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#[cfg(feature = "std")]
use std::collections::HashSet;

#[cfg(not(feature = "std"))]
use alloc::{vec, vec::Vec};
use tree_math::TreeIndex;

use super::node::{Node, NodeIndex};
use crate::client::MlsError;
use crate::crypto::CipherSuiteProvider;
use crate::group::GroupContext;
use crate::iter::wrap_impl_iter;
use crate::tree_kem::math as tree_math;
use crate::tree_kem::{leaf_node_validator::LeafNodeValidator, TreeKemPublic};
use mls_rs_core::identity::IdentityProvider;

#[cfg(all(not(mls_build_async), feature = "rayon"))]
use rayon::prelude::*;

#[cfg(mls_build_async)]
use futures::{StreamExt, TryStreamExt};

pub(crate) struct TreeValidator<'a, C, CSP>
where
    C: IdentityProvider,
    CSP: CipherSuiteProvider,
{
    expected_tree_hash: &'a [u8],
    leaf_node_validator: LeafNodeValidator<'a, C, CSP>,
    group_id: &'a [u8],
    cipher_suite_provider: &'a CSP,
}

impl<'a, C: IdentityProvider, CSP: CipherSuiteProvider> TreeValidator<'a, C, CSP> {
    pub fn new(
        cipher_suite_provider: &'a CSP,
        context: &'a GroupContext,
        identity_provider: &'a C,
    ) -> Self {
        TreeValidator {
            expected_tree_hash: &context.tree_hash,
            leaf_node_validator: LeafNodeValidator::new(
                cipher_suite_provider,
                identity_provider,
                Some(&context.extensions),
            ),
            group_id: &context.group_id,
            cipher_suite_provider,
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn validate(&self, tree: &mut TreeKemPublic) -> Result<(), MlsError> {
        self.validate_tree_hash(tree).await?;

        tree.validate_parent_hashes(self.cipher_suite_provider)
            .await?;

        self.validate_no_trailing_blanks(tree)?;
        self.validate_leaves(tree).await?;
        validate_unmerged(tree)
    }

    fn validate_no_trailing_blanks(&self, tree: &TreeKemPublic) -> Result<(), MlsError> {
        tree.nodes
            .last()
            .ok_or(MlsError::UnexpectedEmptyTree)?
            .is_some()
            .then_some(())
            .ok_or(MlsError::UnexpectedTrailingBlanks)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn validate_tree_hash(&self, tree: &mut TreeKemPublic) -> Result<(), MlsError> {
        //Verify that the tree hash of the ratchet tree matches the tree_hash field in the GroupInfo.
        let tree_hash = tree.tree_hash(self.cipher_suite_provider).await?;

        if tree_hash != self.expected_tree_hash {
            return Err(MlsError::TreeHashMismatch);
        }

        Ok(())
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn validate_leaves(&self, tree: &TreeKemPublic) -> Result<(), MlsError> {
        let leaves = wrap_impl_iter(tree.nodes.non_empty_leaves());

        #[cfg(mls_build_async)]
        let leaves = leaves.map(Ok);

        { leaves }
            .try_for_each(|(index, leaf_node)| async move {
                self.leaf_node_validator
                    .revalidate(leaf_node, self.group_id, *index)
                    .await
            })
            .await
    }
}

fn validate_unmerged(tree: &TreeKemPublic) -> Result<(), MlsError> {
    let unmerged_sets = tree.nodes.iter().map(|n| {
        #[cfg(feature = "std")]
        if let Some(Node::Parent(p)) = n {
            HashSet::from_iter(p.unmerged_leaves.iter().cloned())
        } else {
            HashSet::new()
        }

        #[cfg(not(feature = "std"))]
        if let Some(Node::Parent(p)) = n {
            p.unmerged_leaves.clone()
        } else {
            vec![]
        }
    });

    let mut unmerged_sets = unmerged_sets.collect::<Vec<_>>();

    // For each leaf L, we search for the longest prefix P[1], P[2], ..., P[k] of the direct path of L
    // such that for each i=1..k, either L is in the unmerged leaves of P[i], or P[i] is blank. We will
    // then check that L is unmerged at each P[1], ..., P[k] and no other node.
    let leaf_count = tree.total_leaf_count();

    for (index, _) in tree.nodes.non_empty_leaves() {
        let mut n = NodeIndex::from(index);

        while let Some(ps) = n.parent_sibling(&leaf_count) {
            if tree.nodes.is_blank(ps.parent)? {
                n = ps.parent;
                continue;
            }

            let parent_node = tree.nodes.borrow_as_parent(ps.parent)?;

            if parent_node.unmerged_leaves.contains(&index) {
                unmerged_sets[ps.parent as usize].retain(|i| i != &index);

                n = ps.parent;
            } else {
                break;
            }
        }
    }

    let unmerged_sets = unmerged_sets.iter().all(|set| set.is_empty());

    unmerged_sets
        .then_some(())
        .ok_or(MlsError::UnmergedLeavesMismatch)
}

#[cfg(test)]
mod tests {
    use alloc::vec;
    use assert_matches::assert_matches;

    use super::*;
    use crate::{
        cipher_suite::CipherSuite,
        client::test_utils::TEST_CIPHER_SUITE,
        crypto::test_utils::test_cipher_suite_provider,
        crypto::test_utils::TestCryptoProvider,
        group::test_utils::{get_test_group_context, random_bytes},
        identity::basic::BasicIdentityProvider,
        tree_kem::{
            kem::TreeKem,
            leaf_node::test_utils::{default_properties, get_basic_test_node},
            node::{LeafIndex, Node, Parent},
            parent_hash::{test_utils::get_test_tree_fig_12, ParentHash},
            test_utils::get_test_tree,
        },
    };

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_parent_node(cipher_suite: CipherSuite) -> Parent {
        let (_, public_key) = test_cipher_suite_provider(cipher_suite)
            .kem_generate()
            .await
            .unwrap();

        Parent {
            public_key,
            parent_hash: ParentHash::empty(),
            unmerged_leaves: vec![],
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn get_valid_tree(cipher_suite: CipherSuite) -> TreeKemPublic {
        let cipher_suite_provider = test_cipher_suite_provider(cipher_suite);

        let mut test_tree = get_test_tree(cipher_suite).await;

        let leaf1 = get_basic_test_node(cipher_suite, "leaf1").await;
        let leaf2 = get_basic_test_node(cipher_suite, "leaf2").await;

        test_tree
            .public
            .add_leaves(
                vec![leaf1, leaf2],
                &BasicIdentityProvider,
                &cipher_suite_provider,
            )
            .await
            .unwrap();

        test_tree.public.nodes[1] = Some(Node::Parent(test_parent_node(cipher_suite).await));
        test_tree.public.nodes[3] = Some(Node::Parent(test_parent_node(cipher_suite).await));

        TreeKem::new(&mut test_tree.public, &mut test_tree.private)
            .encap(
                &mut get_test_group_context(42, cipher_suite).await,
                &[LeafIndex(1), LeafIndex(2)],
                &test_tree.creator_signing_key,
                default_properties(),
                None,
                &cipher_suite_provider,
                #[cfg(test)]
                &Default::default(),
            )
            .await
            .unwrap();

        test_tree.public
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_valid_tree() {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let cipher_suite_provider = test_cipher_suite_provider(cipher_suite);

            let mut test_tree = get_valid_tree(cipher_suite).await;

            let mut context = get_test_group_context(1, cipher_suite).await;
            context.tree_hash = test_tree.tree_hash(&cipher_suite_provider).await.unwrap();

            let validator =
                TreeValidator::new(&cipher_suite_provider, &context, &BasicIdentityProvider);

            validator.validate(&mut test_tree).await.unwrap();
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_tree_hash_mismatch() {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let mut test_tree = get_valid_tree(cipher_suite).await;

            let cipher_suite_provider = test_cipher_suite_provider(cipher_suite);
            let context = get_test_group_context(1, cipher_suite).await;

            let validator =
                TreeValidator::new(&cipher_suite_provider, &context, &BasicIdentityProvider);

            let res = validator.validate(&mut test_tree).await;

            assert_matches!(res, Err(MlsError::TreeHashMismatch));
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_parent_hash_mismatch() {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let mut test_tree = get_valid_tree(cipher_suite).await;

            let parent_node = test_tree.nodes.borrow_as_parent_mut(1).unwrap();
            parent_node.parent_hash = ParentHash::from(random_bytes(32));

            let cipher_suite_provider = test_cipher_suite_provider(cipher_suite);
            let mut context = get_test_group_context(1, cipher_suite).await;
            context.tree_hash = test_tree.tree_hash(&cipher_suite_provider).await.unwrap();

            let validator =
                TreeValidator::new(&cipher_suite_provider, &context, &BasicIdentityProvider);

            let res = validator.validate(&mut test_tree).await;

            assert_matches!(res, Err(MlsError::ParentHashMismatch));
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_key_package_validation_failure() {
        for cipher_suite in TestCryptoProvider::all_supported_cipher_suites() {
            let mut test_tree = get_valid_tree(cipher_suite).await;

            test_tree
                .nodes
                .borrow_as_leaf_mut(LeafIndex(0))
                .unwrap()
                .signature = random_bytes(32);

            let cipher_suite_provider = test_cipher_suite_provider(cipher_suite);
            let mut context = get_test_group_context(1, cipher_suite).await;
            context.tree_hash = test_tree.tree_hash(&cipher_suite_provider).await.unwrap();

            let validator =
                TreeValidator::new(&cipher_suite_provider, &context, &BasicIdentityProvider);

            let res = validator.validate(&mut test_tree).await;

            assert_matches!(res, Err(MlsError::InvalidSignature));
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn verify_unmerged_with_correct_tree() {
        let tree = get_test_tree_fig_12(TEST_CIPHER_SUITE).await;
        validate_unmerged(&tree).unwrap();
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn verify_unmerged_with_blank_leaf() {
        let mut tree = get_test_tree_fig_12(TEST_CIPHER_SUITE).await;

        // Blank leaf D unmerged at nodes 3, 7
        tree.nodes[6] = None;

        assert_matches!(
            validate_unmerged(&tree),
            Err(MlsError::UnmergedLeavesMismatch)
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn verify_unmerged_with_broken_path() {
        let mut tree = get_test_tree_fig_12(TEST_CIPHER_SUITE).await;

        // Make D with direct path [3, 7] unmerged at 7 but not 3
        tree.nodes.borrow_as_parent_mut(3).unwrap().unmerged_leaves = vec![];

        assert_matches!(
            validate_unmerged(&tree),
            Err(MlsError::UnmergedLeavesMismatch)
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn verify_unmerged_with_leaf_outside_tree() {
        let mut tree = get_test_tree_fig_12(TEST_CIPHER_SUITE).await;

        // Add leaf E from the right subtree of the root to unmerged leaves of node 1 on the left
        tree.nodes.borrow_as_parent_mut(1).unwrap().unmerged_leaves = vec![LeafIndex(4)];

        assert_matches!(
            validate_unmerged(&tree),
            Err(MlsError::UnmergedLeavesMismatch)
        );
    }
}
