// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::client::MlsError;
use crate::crypto::{CipherSuiteProvider, HpkePublicKey};
use crate::tree_kem::math as tree_math;
use crate::tree_kem::node::{LeafIndex, Node, NodeIndex};
use crate::tree_kem::TreeKemPublic;
use alloc::vec::Vec;
use core::{
    fmt::{self, Debug},
    ops::Deref,
};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::error::IntoAnyError;
use tree_math::TreeIndex;

use super::leaf_node::LeafNodeSource;

#[cfg(feature = "std")]
use std::collections::HashSet;

#[cfg(not(feature = "std"))]
use alloc::collections::BTreeSet;

#[derive(Clone, Debug, MlsSize, MlsEncode)]
struct ParentHashInput<'a> {
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    public_key: &'a HpkePublicKey,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    parent_hash: &'a [u8],
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    original_sibling_tree_hash: &'a [u8],
}

#[derive(Clone, MlsSize, MlsEncode, MlsDecode, PartialEq, Eq)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct ParentHash(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    Vec<u8>,
);

impl Debug for ParentHash {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("ParentHash")
            .fmt(f)
    }
}

impl From<Vec<u8>> for ParentHash {
    fn from(v: Vec<u8>) -> Self {
        Self(v)
    }
}

impl Deref for ParentHash {
    type Target = Vec<u8>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl ParentHash {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn new<P: CipherSuiteProvider>(
        cipher_suite_provider: &P,
        public_key: &HpkePublicKey,
        parent_hash: &ParentHash,
        original_sibling_tree_hash: &[u8],
    ) -> Result<Self, MlsError> {
        let input = ParentHashInput {
            public_key,
            parent_hash,
            original_sibling_tree_hash,
        };

        let input_bytes = input.mls_encode_to_vec()?;

        let hash = cipher_suite_provider
            .hash(&input_bytes)
            .await
            .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))?;

        Ok(Self(hash))
    }

    pub fn empty() -> Self {
        ParentHash(Vec::new())
    }

    pub fn matches(&self, hash: &ParentHash) -> bool {
        //TODO: Constant time equals
        hash == self
    }
}

impl Node {
    fn get_parent_hash(&self) -> Option<ParentHash> {
        match self {
            Node::Parent(p) => Some(p.parent_hash.clone()),
            Node::Leaf(l) => match &l.leaf_node_source {
                LeafNodeSource::Commit(parent_hash) => Some(parent_hash.clone()),
                _ => None,
            },
        }
    }
}

impl TreeKemPublic {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn parent_hash_for_leaf<P: CipherSuiteProvider>(
        &mut self,
        cipher_suite_provider: &P,
        index: LeafIndex,
    ) -> Result<ParentHash, MlsError> {
        let mut hash = ParentHash::empty();

        for node in self.nodes.direct_copath(index).into_iter().rev() {
            if self.nodes.is_resolution_empty(node.copath) {
                continue;
            }

            let parent = self.nodes.borrow_as_parent_mut(node.path)?;

            let calculated = ParentHash::new(
                cipher_suite_provider,
                &parent.public_key,
                &hash,
                &self.tree_hashes.current[node.copath as usize],
            )
            .await?;

            (parent.parent_hash, hash) = (hash, calculated);
        }

        Ok(hash)
    }

    // Updates all of the required parent hash values, and returns the calculated parent hash value for the leaf node
    // If an update path is provided, additionally verify that the calculated parent hash matches
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn update_parent_hashes<P: CipherSuiteProvider>(
        &mut self,
        index: LeafIndex,
        verify_leaf_hash: bool,
        cipher_suite_provider: &P,
    ) -> Result<(), MlsError> {
        // First update the relevant original hashes used for parent hash computation.
        self.update_hashes(&[index], cipher_suite_provider).await?;

        let leaf_hash = self
            .parent_hash_for_leaf(cipher_suite_provider, index)
            .await?;

        let leaf = self.nodes.borrow_as_leaf_mut(index)?;

        if verify_leaf_hash {
            // Verify the parent hash of the new sender leaf node and update the parent hash values
            // in the local tree
            if let LeafNodeSource::Commit(parent_hash) = &leaf.leaf_node_source {
                if !leaf_hash.matches(parent_hash) {
                    return Err(MlsError::ParentHashMismatch);
                }
            } else {
                return Err(MlsError::InvalidLeafNodeSource);
            }
        } else {
            leaf.leaf_node_source = LeafNodeSource::Commit(leaf_hash);
        }

        // Update hashes after changes to the tree.
        self.update_hashes(&[index], cipher_suite_provider).await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(super) async fn validate_parent_hashes<P: CipherSuiteProvider>(
        &self,
        cipher_suite_provider: &P,
    ) -> Result<(), MlsError> {
        let original_hashes = self.compute_original_hashes(cipher_suite_provider).await?;

        let nodes_to_validate = self
            .nodes
            .non_empty_parents()
            .map(|(node_index, _)| node_index);

        #[cfg(feature = "std")]
        let mut nodes_to_validate = nodes_to_validate.collect::<HashSet<_>>();
        #[cfg(not(feature = "std"))]
        let mut nodes_to_validate = nodes_to_validate.collect::<BTreeSet<_>>();

        let num_leaves = self.total_leaf_count();

        // For each leaf l, validate all non-blank nodes on the chain from l up the tree.
        for (leaf_index, _) in self.nodes.non_empty_leaves() {
            let mut n = NodeIndex::from(leaf_index);

            while let Some(mut ps) = n.parent_sibling(&num_leaves) {
                // Find the first non-blank ancestor p of n and p's co-path child s.
                while self.nodes.is_blank(ps.parent)? {
                    // If we reached the root, we're done with this chain.
                    let Some(ps_parent) = ps.parent.parent_sibling(&num_leaves) else {
                        return Ok(());
                    };

                    ps = ps_parent;
                }

                // Check is n's parent_hash field matches the parent hash of p with co-path child s.
                let p_parent = self.nodes.borrow_as_parent(ps.parent)?;

                let n_node = self
                    .nodes
                    .borrow_node(n)?
                    .as_ref()
                    .ok_or(MlsError::ExpectedNode)?;

                let calculated = ParentHash::new(
                    cipher_suite_provider,
                    &p_parent.public_key,
                    &p_parent.parent_hash,
                    &original_hashes[ps.sibling as usize],
                )
                .await?;

                if n_node.get_parent_hash() == Some(calculated) {
                    // Check that "n is in the resolution of c, and the intersection of p's unmerged_leaves with the subtree
                    // under c is equal to the resolution of c with n removed".
                    let Some(cp) = ps.sibling.parent_sibling(&num_leaves) else {
                        return Err(MlsError::ParentHashMismatch);
                    };

                    let c = cp.sibling;
                    let c_resolution = self.nodes.get_resolution_index(c)?.into_iter();

                    #[cfg(feature = "std")]
                    let mut c_resolution = c_resolution.collect::<HashSet<_>>();
                    #[cfg(not(feature = "std"))]
                    let mut c_resolution = c_resolution.collect::<BTreeSet<_>>();

                    let p_unmerged_in_c_subtree = self
                        .unmerged_in_subtree(ps.parent, c)?
                        .iter()
                        .copied()
                        .map(|x| *x * 2);

                    #[cfg(feature = "std")]
                    let p_unmerged_in_c_subtree = p_unmerged_in_c_subtree.collect::<HashSet<_>>();
                    #[cfg(not(feature = "std"))]
                    let p_unmerged_in_c_subtree = p_unmerged_in_c_subtree.collect::<BTreeSet<_>>();

                    if c_resolution.remove(&n)
                        && c_resolution == p_unmerged_in_c_subtree
                        && nodes_to_validate.remove(&ps.parent)
                    {
                        // If n's parent_hash field matches and p has not been validated yet, mark p as validated and continue.
                        n = ps.parent;
                    } else {
                        // If p is validated for the second time, the check fails ("all non-blank parent nodes are covered by exactly one such chain").
                        return Err(MlsError::ParentHashMismatch);
                    }
                } else {
                    // If n's parent_hash field doesn't match, we're done with this chain.
                    break;
                }
            }
        }

        // The check passes iff all non-blank nodes are validated.
        if nodes_to_validate.is_empty() {
            Ok(())
        } else {
            Err(MlsError::ParentHashMismatch)
        }
    }
}

#[cfg(test)]
pub(crate) mod test_utils {

    use super::*;
    use crate::{
        cipher_suite::CipherSuite,
        crypto::test_utils::test_cipher_suite_provider,
        identity::basic::BasicIdentityProvider,
        tree_kem::{leaf_node::test_utils::get_basic_test_node, node::Parent},
    };

    use alloc::vec;

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn test_parent(
        cipher_suite: CipherSuite,
        unmerged_leaves: Vec<LeafIndex>,
    ) -> Parent {
        let (_, public_key) = test_cipher_suite_provider(cipher_suite)
            .kem_generate()
            .await
            .unwrap();

        Parent {
            public_key,
            parent_hash: ParentHash::empty(),
            unmerged_leaves,
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn test_parent_node(
        cipher_suite: CipherSuite,
        unmerged_leaves: Vec<LeafIndex>,
    ) -> Node {
        Node::Parent(test_parent(cipher_suite, unmerged_leaves).await)
    }

    // Create figure 12 from MLS RFC
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn get_test_tree_fig_12(cipher_suite: CipherSuite) -> TreeKemPublic {
        let cipher_suite_provider = test_cipher_suite_provider(cipher_suite);

        let mut tree = TreeKemPublic::new();

        let mut leaves = Vec::new();

        for l in ["A", "B", "C", "D", "E", "F", "G"] {
            leaves.push(get_basic_test_node(cipher_suite, l).await);
        }

        tree.add_leaves(leaves, &BasicIdentityProvider, &cipher_suite_provider)
            .await
            .unwrap();

        tree.nodes[1] = Some(test_parent_node(cipher_suite, vec![]).await);
        tree.nodes[3] = Some(test_parent_node(cipher_suite, vec![LeafIndex(3)]).await);

        tree.nodes[7] =
            Some(test_parent_node(cipher_suite, vec![LeafIndex(3), LeafIndex(6)]).await);

        tree.nodes[9] = Some(test_parent_node(cipher_suite, vec![LeafIndex(5)]).await);

        tree.nodes[11] =
            Some(test_parent_node(cipher_suite, vec![LeafIndex(5), LeafIndex(6)]).await);

        tree.update_parent_hashes(LeafIndex(0), false, &cipher_suite_provider)
            .await
            .unwrap();

        tree.update_parent_hashes(LeafIndex(4), false, &cipher_suite_provider)
            .await
            .unwrap();

        tree
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::client::test_utils::TEST_CIPHER_SUITE;
    use crate::crypto::test_utils::test_cipher_suite_provider;
    use crate::tree_kem::leaf_node::test_utils::get_basic_test_node;
    use crate::tree_kem::leaf_node::LeafNodeSource;
    use crate::tree_kem::test_utils::TreeWithSigners;
    use crate::tree_kem::MlsError;
    use assert_matches::assert_matches;

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_missing_parent_hash() {
        let cs = test_cipher_suite_provider(TEST_CIPHER_SUITE);
        let mut test_tree = TreeWithSigners::make_full_tree(8, &cs).await.tree;

        *test_tree.nodes.borrow_as_leaf_mut(LeafIndex(0)).unwrap() =
            get_basic_test_node(TEST_CIPHER_SUITE, "foo").await;

        let missing_parent_hash_res = test_tree
            .update_parent_hashes(
                LeafIndex(0),
                true,
                &test_cipher_suite_provider(TEST_CIPHER_SUITE),
            )
            .await;

        assert_matches!(
            missing_parent_hash_res,
            Err(MlsError::InvalidLeafNodeSource)
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_parent_hash_mismatch() {
        let cs = test_cipher_suite_provider(TEST_CIPHER_SUITE);
        let mut test_tree = TreeWithSigners::make_full_tree(8, &cs).await.tree;

        let unexpected_parent_hash = ParentHash::from(hex!("f00d"));

        test_tree
            .nodes
            .borrow_as_leaf_mut(LeafIndex(0))
            .unwrap()
            .leaf_node_source = LeafNodeSource::Commit(unexpected_parent_hash);

        let invalid_parent_hash_res = test_tree
            .update_parent_hashes(
                LeafIndex(0),
                true,
                &test_cipher_suite_provider(TEST_CIPHER_SUITE),
            )
            .await;

        assert_matches!(invalid_parent_hash_res, Err(MlsError::ParentHashMismatch));
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_parent_hash_invalid() {
        let cs = test_cipher_suite_provider(TEST_CIPHER_SUITE);
        let mut test_tree = TreeWithSigners::make_full_tree(8, &cs).await.tree;

        test_tree.nodes[2] = None;

        let res = test_tree
            .validate_parent_hashes(&test_cipher_suite_provider(TEST_CIPHER_SUITE))
            .await;

        assert_matches!(res, Err(MlsError::ParentHashMismatch));
    }
}
