// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use super::leaf_node::LeafNode;
use super::node::{LeafIndex, NodeVec};
use super::tree_math::BfsIterTopDown;
use crate::client::MlsError;
use crate::crypto::CipherSuiteProvider;
use crate::tree_kem::math as tree_math;
use crate::tree_kem::node::Parent;
use crate::tree_kem::TreeKemPublic;
use alloc::collections::VecDeque;
use alloc::vec;
use alloc::vec::Vec;
use core::fmt::{self, Debug};
use itertools::Itertools;
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::error::IntoAnyError;
use tree_math::TreeIndex;

use core::ops::Deref;

#[derive(Clone, Default, MlsSize, MlsEncode, MlsDecode, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct TreeHash(
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    #[cfg_attr(feature = "serde", serde(with = "mls_rs_core::vec_serde"))]
    Vec<u8>,
);

impl Debug for TreeHash {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("TreeHash")
            .fmt(f)
    }
}

impl Deref for TreeHash {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

#[derive(Clone, Debug, Default, MlsSize, MlsEncode, MlsDecode, PartialEq)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct TreeHashes {
    pub current: Vec<TreeHash>,
}

#[derive(Debug, MlsSize, MlsEncode)]
struct LeafNodeHashInput<'a> {
    leaf_index: LeafIndex,
    leaf_node: Option<&'a LeafNode>,
}

#[derive(Debug, MlsSize, MlsEncode)]
struct ParentNodeTreeHashInput<'a> {
    parent_node: Option<&'a Parent>,
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    left_hash: &'a [u8],
    #[mls_codec(with = "mls_rs_codec::byte_vec")]
    right_hash: &'a [u8],
}

#[derive(Debug, MlsSize, MlsEncode)]
#[repr(u8)]
enum TreeHashInput<'a> {
    Leaf(LeafNodeHashInput<'a>) = 1u8,
    Parent(ParentNodeTreeHashInput<'a>) = 2u8,
}

impl TreeKemPublic {
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    #[inline(never)]
    pub async fn tree_hash<P: CipherSuiteProvider>(
        &mut self,
        cipher_suite_provider: &P,
    ) -> Result<Vec<u8>, MlsError> {
        self.initialize_hashes(cipher_suite_provider).await?;
        let root = self.total_leaf_count().root();
        Ok(self.tree_hashes.current[root as usize].to_vec())
    }

    // Update hashes after `committer` makes changes to the tree. `path_blank` is the
    // list of leaves whose paths were blanked, i.e. updates and removes.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn update_hashes<P: CipherSuiteProvider>(
        &mut self,
        updated_leaves: &[LeafIndex],
        cipher_suite_provider: &P,
    ) -> Result<(), MlsError> {
        let num_leaves = self.total_leaf_count();

        let trailing_blanks = (0..num_leaves)
            .rev()
            .map_while(|l| {
                self.tree_hashes
                    .current
                    .get(2 * l as usize)
                    .is_none()
                    .then_some(LeafIndex(l))
            })
            .collect::<Vec<_>>();

        // Update the current hashes for direct paths of all modified leaves.
        tree_hash(
            &mut self.tree_hashes.current,
            &self.nodes,
            Some([updated_leaves, &trailing_blanks].concat()),
            &[],
            num_leaves,
            cipher_suite_provider,
        )
        .await?;

        Ok(())
    }

    // Initialize all hashes after creating / importing a tree.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn initialize_hashes<P>(&mut self, cipher_suite_provider: &P) -> Result<(), MlsError>
    where
        P: CipherSuiteProvider,
    {
        if self.tree_hashes.current.is_empty() {
            let num_leaves = self.total_leaf_count();

            tree_hash(
                &mut self.tree_hashes.current,
                &self.nodes,
                None,
                &[],
                num_leaves,
                cipher_suite_provider,
            )
            .await?;
        }

        Ok(())
    }

    pub(crate) fn unmerged_in_subtree(
        &self,
        node_unmerged: u32,
        subtree_root: u32,
    ) -> Result<&[LeafIndex], MlsError> {
        let unmerged = &self.nodes.borrow_as_parent(node_unmerged)?.unmerged_leaves;
        let (left, right) = tree_math::subtree(subtree_root);
        let mut start = 0;
        while start < unmerged.len() && unmerged[start] < left {
            start += 1;
        }
        let mut end = start;
        while end < unmerged.len() && unmerged[end] < right {
            end += 1;
        }
        Ok(&unmerged[start..end])
    }

    fn different_unmerged(&self, ancestor: u32, descendant: u32) -> Result<bool, MlsError> {
        Ok(!self.nodes.is_blank(ancestor)?
            && !self.nodes.is_blank(descendant)?
            && self.unmerged_in_subtree(ancestor, descendant)?
                != self.nodes.borrow_as_parent(descendant)?.unmerged_leaves)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn compute_original_hashes<P: CipherSuiteProvider>(
        &self,
        cipher_suite: &P,
    ) -> Result<Vec<TreeHash>, MlsError> {
        let num_leaves = self.nodes.total_leaf_count() as usize;
        let root = (num_leaves as u32).root();

        // The value `filtered_sets[n]` is a list of all ancestors `a` of `n` s.t. we have to compute
        // the tree hash of `n` with the unmerged leaves of `a` filtered out.
        let mut filtered_sets = vec![vec![]; num_leaves * 2 - 1];
        filtered_sets[root as usize].push(root);
        let mut tree_hashes = vec![vec![]; num_leaves * 2 - 1];

        let bfs_iter = BfsIterTopDown::new(num_leaves).skip(1);

        for n in bfs_iter {
            let Some(ps) = (n as u32).parent_sibling(&(num_leaves as u32)) else {
                break;
            };

            let p = ps.parent;

            // Clippy's suggestion `filtered_sets[n].clone_from(&filtered_sets[p as usize])` is wrong and does not compile
            #[allow(clippy::assigning_clones)]
            {
                filtered_sets[n] = filtered_sets[p as usize].clone();
            }

            if self.different_unmerged(*filtered_sets[p as usize].last().unwrap(), p)? {
                filtered_sets[n].push(p);

                // Compute tree hash of `n` without unmerged leaves of `p`. This also computes the tree hash
                // for any descendants of `n` added to `filtered_sets` later via `clone`.
                let (start_leaf, end_leaf) = tree_math::subtree(n as u32);

                tree_hash(
                    &mut tree_hashes[p as usize],
                    &self.nodes,
                    Some((*start_leaf..*end_leaf).map(LeafIndex).collect_vec()),
                    &self.nodes.borrow_as_parent(p)?.unmerged_leaves,
                    num_leaves as u32,
                    cipher_suite,
                )
                .await?;
            }
        }

        // Set the `original_hashes` based on the computed `hashes`.
        let mut original_hashes = vec![TreeHash::default(); num_leaves * 2 - 1];

        // If root has unmerged leaves, we recompute it's original hash. Else, we can use the current hash.
        let root_original = if !self.nodes.is_blank(root)? && !self.nodes.is_leaf(root) {
            let root_unmerged = &self.nodes.borrow_as_parent(root)?.unmerged_leaves;

            if !root_unmerged.is_empty() {
                let mut hashes = vec![];

                tree_hash(
                    &mut hashes,
                    &self.nodes,
                    None,
                    root_unmerged,
                    num_leaves as u32,
                    cipher_suite,
                )
                .await?;

                Some(hashes)
            } else {
                None
            }
        } else {
            None
        };

        for (i, hash) in original_hashes.iter_mut().enumerate() {
            let a = filtered_sets[i].last().unwrap();
            *hash = if self.nodes.is_blank(*a)? || a == &root {
                if let Some(root_original) = &root_original {
                    root_original[i].clone()
                } else {
                    self.tree_hashes.current[i].clone()
                }
            } else {
                tree_hashes[*a as usize][i].clone()
            }
        }

        Ok(original_hashes)
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn tree_hash<P: CipherSuiteProvider>(
    hashes: &mut Vec<TreeHash>,
    nodes: &NodeVec,
    leaves_to_update: Option<Vec<LeafIndex>>,
    filtered_leaves: &[LeafIndex],
    num_leaves: u32,
    cipher_suite_provider: &P,
) -> Result<(), MlsError> {
    let leaves_to_update =
        leaves_to_update.unwrap_or_else(|| (0..num_leaves).map(LeafIndex).collect::<Vec<_>>());

    // Resize the array in case the tree was extended or truncated
    hashes.resize(num_leaves as usize * 2 - 1, TreeHash::default());

    let mut node_queue = VecDeque::with_capacity(leaves_to_update.len());

    for l in leaves_to_update.iter().filter(|l| ***l < num_leaves) {
        let leaf = (!filtered_leaves.contains(l))
            .then_some(nodes.borrow_as_leaf(*l).ok())
            .flatten();

        hashes[2 * **l as usize] = TreeHash(hash_for_leaf(*l, leaf, cipher_suite_provider).await?);

        if let Some(ps) = (2 * **l).parent_sibling(&num_leaves) {
            node_queue.push_back(ps.parent);
        }
    }

    while let Some(n) = node_queue.pop_front() {
        let hash = TreeHash(
            hash_for_parent(
                nodes.borrow_as_parent(n).ok(),
                cipher_suite_provider,
                filtered_leaves,
                &hashes[n.left_unchecked() as usize],
                &hashes[n.right_unchecked() as usize],
            )
            .await?,
        );

        hashes[n as usize] = hash;

        if let Some(ps) = n.parent_sibling(&num_leaves) {
            node_queue.push_back(ps.parent);
        }
    }

    Ok(())
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn hash_for_leaf<P: CipherSuiteProvider>(
    leaf_index: LeafIndex,
    leaf_node: Option<&LeafNode>,
    cipher_suite_provider: &P,
) -> Result<Vec<u8>, MlsError> {
    let input = TreeHashInput::Leaf(LeafNodeHashInput {
        leaf_index,
        leaf_node,
    });

    cipher_suite_provider
        .hash(&input.mls_encode_to_vec()?)
        .await
        .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
async fn hash_for_parent<P: CipherSuiteProvider>(
    parent_node: Option<&Parent>,
    cipher_suite_provider: &P,
    filtered: &[LeafIndex],
    left_hash: &[u8],
    right_hash: &[u8],
) -> Result<Vec<u8>, MlsError> {
    let mut parent_node = parent_node.cloned();

    if let Some(ref mut parent_node) = parent_node {
        parent_node
            .unmerged_leaves
            .retain(|unmerged_index| !filtered.contains(unmerged_index));
    }

    let input = TreeHashInput::Parent(ParentNodeTreeHashInput {
        parent_node: parent_node.as_ref(),
        left_hash,
        right_hash,
    });

    cipher_suite_provider
        .hash(&input.mls_encode_to_vec()?)
        .await
        .map_err(|e| MlsError::CryptoProviderError(e.into_any_error()))
}

#[cfg(test)]
mod tests {
    use mls_rs_codec::MlsDecode;

    use crate::{
        cipher_suite::CipherSuite,
        crypto::test_utils::{test_cipher_suite_provider, try_test_cipher_suite_provider},
        identity::basic::BasicIdentityProvider,
        tree_kem::{node::NodeVec, parent_hash::test_utils::get_test_tree_fig_12},
    };

    use super::*;

    #[derive(serde::Deserialize, serde::Serialize)]
    struct TestCase {
        cipher_suite: u16,
        #[serde(with = "hex::serde")]
        tree_data: Vec<u8>,
        #[serde(with = "hex::serde")]
        tree_hash: Vec<u8>,
    }

    impl TestCase {
        #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
        #[cfg_attr(coverage_nightly, coverage(off))]
        async fn generate() -> Vec<TestCase> {
            let mut test_cases = Vec::new();

            for cipher_suite in CipherSuite::all() {
                let mut tree = get_test_tree_fig_12(cipher_suite).await;

                test_cases.push(TestCase {
                    cipher_suite: cipher_suite.into(),
                    tree_data: tree.nodes.mls_encode_to_vec().unwrap(),
                    tree_hash: tree
                        .tree_hash(&test_cipher_suite_provider(cipher_suite))
                        .await
                        .unwrap(),
                })
            }

            test_cases
        }
    }

    #[cfg(mls_build_async)]
    async fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(tree_hash, TestCase::generate().await)
    }

    #[cfg(not(mls_build_async))]
    fn load_test_cases() -> Vec<TestCase> {
        load_test_case_json!(tree_hash, TestCase::generate())
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_tree_hash() {
        let cases = load_test_cases().await;

        for one_case in cases {
            let Some(cs_provider) = try_test_cipher_suite_provider(one_case.cipher_suite) else {
                continue;
            };

            let mut tree = TreeKemPublic::import_node_data(
                NodeVec::mls_decode(&mut &*one_case.tree_data).unwrap(),
                &BasicIdentityProvider,
                &Default::default(),
            )
            .await
            .unwrap();

            let calculated_hash = tree.tree_hash(&cs_provider).await.unwrap();

            assert_eq!(calculated_hash, one_case.tree_hash);
        }
    }
}
