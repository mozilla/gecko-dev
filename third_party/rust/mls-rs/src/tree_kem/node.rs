// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use super::leaf_node::LeafNode;
use crate::client::MlsError;
use crate::crypto::HpkePublicKey;
use crate::tree_kem::math as tree_math;
use crate::tree_kem::parent_hash::ParentHash;
use alloc::vec;
use alloc::vec::Vec;
use core::hash::Hash;
use core::ops::{Deref, DerefMut};
use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use tree_math::{CopathNode, TreeIndex};

#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct Parent {
    pub public_key: HpkePublicKey,
    pub parent_hash: ParentHash,
    pub unmerged_leaves: Vec<LeafIndex>,
}

#[derive(
    Clone, Copy, Debug, Ord, PartialEq, PartialOrd, Hash, Eq, MlsSize, MlsEncode, MlsDecode,
)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct LeafIndex(pub(crate) u32);

impl LeafIndex {
    pub fn new(i: u32) -> Self {
        Self(i)
    }
}

impl Deref for LeafIndex {
    type Target = u32;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl From<&LeafIndex> for NodeIndex {
    fn from(leaf_index: &LeafIndex) -> Self {
        leaf_index.0 * 2
    }
}

impl From<LeafIndex> for NodeIndex {
    fn from(leaf_index: LeafIndex) -> Self {
        leaf_index.0 * 2
    }
}

pub(crate) type NodeIndex = u32;

#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode, MlsDecode)]
#[allow(clippy::large_enum_variant)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(u8)]
//TODO: Research if this should actually be a Box<Leaf> for memory / performance reasons
pub(crate) enum Node {
    Leaf(LeafNode) = 1u8,
    Parent(Parent) = 2u8,
}

impl Node {
    pub fn public_key(&self) -> &HpkePublicKey {
        match self {
            Node::Parent(p) => &p.public_key,
            Node::Leaf(l) => &l.public_key,
        }
    }
}

impl From<Parent> for Option<Node> {
    fn from(p: Parent) -> Self {
        Node::from(p).into()
    }
}

impl From<LeafNode> for Option<Node> {
    fn from(l: LeafNode) -> Self {
        Node::from(l).into()
    }
}

impl From<Parent> for Node {
    fn from(p: Parent) -> Self {
        Node::Parent(p)
    }
}

impl From<LeafNode> for Node {
    fn from(l: LeafNode) -> Self {
        Node::Leaf(l)
    }
}

pub(crate) trait NodeTypeResolver {
    fn as_parent(&self) -> Result<&Parent, MlsError>;
    fn as_parent_mut(&mut self) -> Result<&mut Parent, MlsError>;
    fn as_leaf(&self) -> Result<&LeafNode, MlsError>;
    fn as_leaf_mut(&mut self) -> Result<&mut LeafNode, MlsError>;
    fn as_non_empty(&self) -> Result<&Node, MlsError>;
}

impl NodeTypeResolver for Option<Node> {
    fn as_parent(&self) -> Result<&Parent, MlsError> {
        self.as_ref()
            .and_then(|n| match n {
                Node::Parent(p) => Some(p),
                Node::Leaf(_) => None,
            })
            .ok_or(MlsError::ExpectedNode)
    }

    fn as_parent_mut(&mut self) -> Result<&mut Parent, MlsError> {
        self.as_mut()
            .and_then(|n| match n {
                Node::Parent(p) => Some(p),
                Node::Leaf(_) => None,
            })
            .ok_or(MlsError::ExpectedNode)
    }

    fn as_leaf(&self) -> Result<&LeafNode, MlsError> {
        self.as_ref()
            .and_then(|n| match n {
                Node::Parent(_) => None,
                Node::Leaf(l) => Some(l),
            })
            .ok_or(MlsError::ExpectedNode)
    }

    fn as_leaf_mut(&mut self) -> Result<&mut LeafNode, MlsError> {
        self.as_mut()
            .and_then(|n| match n {
                Node::Parent(_) => None,
                Node::Leaf(l) => Some(l),
            })
            .ok_or(MlsError::ExpectedNode)
    }

    fn as_non_empty(&self) -> Result<&Node, MlsError> {
        self.as_ref().ok_or(MlsError::UnexpectedEmptyNode)
    }
}

#[derive(Clone, Debug, PartialEq, MlsSize, MlsEncode, MlsDecode, Default)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct NodeVec(Vec<Option<Node>>);

impl From<Vec<Option<Node>>> for NodeVec {
    fn from(x: Vec<Option<Node>>) -> Self {
        NodeVec(x)
    }
}

impl Deref for NodeVec {
    type Target = Vec<Option<Node>>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for NodeVec {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl NodeVec {
    #[cfg(any(test, all(feature = "custom_proposal", feature = "tree_index")))]
    pub fn occupied_leaf_count(&self) -> u32 {
        self.non_empty_leaves().count() as u32
    }

    pub fn total_leaf_count(&self) -> u32 {
        (self.len() as u32 / 2 + 1).next_power_of_two()
    }

    #[inline]
    pub fn borrow_node(&self, index: NodeIndex) -> Result<&Option<Node>, MlsError> {
        Ok(self.get(self.validate_index(index)?).unwrap_or(&None))
    }

    fn validate_index(&self, index: NodeIndex) -> Result<usize, MlsError> {
        if (index as usize) >= self.len().next_power_of_two() {
            Err(MlsError::InvalidNodeIndex(index))
        } else {
            Ok(index as usize)
        }
    }

    #[cfg(test)]
    fn empty_leaves(&mut self) -> impl Iterator<Item = (LeafIndex, &mut Option<Node>)> {
        self.iter_mut()
            .step_by(2)
            .enumerate()
            .filter(|(_, n)| n.is_none())
            .map(|(i, n)| (LeafIndex(i as u32), n))
    }

    pub fn non_empty_leaves(&self) -> impl Iterator<Item = (LeafIndex, &LeafNode)> + '_ {
        self.leaves()
            .enumerate()
            .filter_map(|(i, l)| l.map(|l| (LeafIndex(i as u32), l)))
    }

    pub fn non_empty_parents(&self) -> impl Iterator<Item = (NodeIndex, &Parent)> + '_ {
        self.iter()
            .enumerate()
            .skip(1)
            .step_by(2)
            .map(|(i, n)| (i as NodeIndex, n))
            .filter_map(|(i, n)| n.as_parent().ok().map(|p| (i, p)))
    }

    pub fn leaves(&self) -> impl Iterator<Item = Option<&LeafNode>> + '_ {
        self.iter().step_by(2).map(|n| n.as_leaf().ok())
    }

    pub fn direct_copath(&self, index: LeafIndex) -> Vec<CopathNode<NodeIndex>> {
        NodeIndex::from(index).direct_copath(&self.total_leaf_count())
    }

    // Section 8.4
    // The filtered direct path of a node is obtained from the node's direct path by removing
    // all nodes whose child on the nodes's copath has an empty resolution
    pub fn filtered(&self, index: LeafIndex) -> Result<Vec<bool>, MlsError> {
        Ok(NodeIndex::from(index)
            .direct_copath(&self.total_leaf_count())
            .into_iter()
            .map(|cp| self.is_resolution_empty(cp.copath))
            .collect())
    }

    #[inline]
    pub fn is_blank(&self, index: NodeIndex) -> Result<bool, MlsError> {
        self.borrow_node(index).map(|n| n.is_none())
    }

    #[inline]
    pub fn is_leaf(&self, index: NodeIndex) -> bool {
        index % 2 == 0
    }

    // Blank a previously filled leaf node, and return the existing leaf
    pub fn blank_leaf_node(&mut self, leaf_index: LeafIndex) -> Result<LeafNode, MlsError> {
        let node_index = self.validate_index(leaf_index.into())?;

        match self.get_mut(node_index).and_then(Option::take) {
            Some(Node::Leaf(l)) => Ok(l),
            _ => Err(MlsError::RemovingNonExistingMember),
        }
    }

    pub fn blank_direct_path(&mut self, leaf: LeafIndex) -> Result<(), MlsError> {
        for i in self.direct_copath(leaf) {
            if let Some(n) = self.get_mut(i.path as usize) {
                *n = None
            }
        }

        Ok(())
    }

    // Remove elements until the last node is non-blank
    pub fn trim(&mut self) {
        while self.last() == Some(&None) {
            self.pop();
        }
    }

    pub fn borrow_as_parent(&self, node_index: NodeIndex) -> Result<&Parent, MlsError> {
        self.borrow_node(node_index).and_then(|n| n.as_parent())
    }

    pub fn borrow_as_parent_mut(&mut self, node_index: NodeIndex) -> Result<&mut Parent, MlsError> {
        let index = self.validate_index(node_index)?;

        self.get_mut(index)
            .ok_or(MlsError::InvalidNodeIndex(node_index))?
            .as_parent_mut()
    }

    pub fn borrow_as_leaf_mut(&mut self, index: LeafIndex) -> Result<&mut LeafNode, MlsError> {
        let node_index = NodeIndex::from(index);
        let index = self.validate_index(node_index)?;

        self.get_mut(index)
            .ok_or(MlsError::InvalidNodeIndex(node_index))?
            .as_leaf_mut()
    }

    pub fn borrow_as_leaf(&self, index: LeafIndex) -> Result<&LeafNode, MlsError> {
        let node_index = NodeIndex::from(index);
        self.borrow_node(node_index).and_then(|n| n.as_leaf())
    }

    pub fn borrow_or_fill_node_as_parent(
        &mut self,
        node_index: NodeIndex,
        public_key: &HpkePublicKey,
    ) -> Result<&mut Parent, MlsError> {
        let index = self.validate_index(node_index)?;

        while self.len() <= index {
            self.push(None);
        }

        self.get_mut(index)
            .ok_or(MlsError::InvalidNodeIndex(node_index))
            .and_then(|n| {
                if n.is_none() {
                    *n = Parent {
                        public_key: public_key.clone(),
                        parent_hash: ParentHash::empty(),
                        unmerged_leaves: vec![],
                    }
                    .into();
                }
                n.as_parent_mut()
            })
    }

    pub fn get_resolution_index(&self, index: NodeIndex) -> Result<Vec<NodeIndex>, MlsError> {
        let mut indexes = vec![index];
        let mut resolution = vec![];

        while let Some(index) = indexes.pop() {
            if let Some(Some(node)) = self.get(index as usize) {
                resolution.push(index);

                if let Node::Parent(p) = node {
                    resolution.extend(p.unmerged_leaves.iter().map(NodeIndex::from));
                }
            } else if !index.is_leaf() {
                indexes.push(index.right_unchecked());
                indexes.push(index.left_unchecked());
            }
        }

        Ok(resolution)
    }

    pub fn find_in_resolution(
        &self,
        index: NodeIndex,
        to_find: Option<NodeIndex>,
    ) -> Option<usize> {
        let mut indexes = vec![index];
        let mut resolution_len = 0;

        while let Some(index) = indexes.pop() {
            if let Some(Some(node)) = self.get(index as usize) {
                if Some(index) == to_find || to_find.is_none() {
                    return Some(resolution_len);
                }

                resolution_len += 1;

                if let Node::Parent(p) = node {
                    indexes.extend(p.unmerged_leaves.iter().map(NodeIndex::from));
                }
            } else if !index.is_leaf() {
                indexes.push(index.right_unchecked());
                indexes.push(index.left_unchecked());
            }
        }

        None
    }

    pub fn is_resolution_empty(&self, index: NodeIndex) -> bool {
        self.find_in_resolution(index, None).is_none()
    }

    pub(crate) fn next_empty_leaf(&self, start: LeafIndex) -> LeafIndex {
        let mut n = NodeIndex::from(start) as usize;

        while n < self.len() {
            if self.0[n].is_none() {
                return LeafIndex((n as u32) >> 1);
            }

            n += 2;
        }

        LeafIndex((self.len() as u32 + 1) >> 1)
    }

    /// If `index` fits in the current tree, inserts `leaf` at `index`. Else, inserts `leaf` as the
    /// last leaf
    pub fn insert_leaf(&mut self, index: LeafIndex, leaf: LeafNode) {
        let node_index = (*index as usize) << 1;

        if node_index > self.len() {
            self.push(None);
            self.push(None);
        } else if self.is_empty() {
            self.push(None);
        }

        self.0[node_index] = Some(leaf.into());
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    use super::*;
    use crate::{
        client::test_utils::TEST_CIPHER_SUITE, tree_kem::leaf_node::test_utils::get_basic_test_node,
    };

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn get_test_node_vec() -> NodeVec {
        let mut nodes = vec![None; 7];

        nodes[0] = get_basic_test_node(TEST_CIPHER_SUITE, "A").await.into();
        nodes[4] = get_basic_test_node(TEST_CIPHER_SUITE, "C").await.into();

        nodes[5] = Parent {
            public_key: b"CD".to_vec().into(),
            parent_hash: ParentHash::empty(),
            unmerged_leaves: vec![LeafIndex(2)],
        }
        .into();

        nodes[6] = get_basic_test_node(TEST_CIPHER_SUITE, "D").await.into();

        NodeVec::from(nodes)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        client::test_utils::TEST_CIPHER_SUITE,
        tree_kem::{
            leaf_node::test_utils::get_basic_test_node, node::test_utils::get_test_node_vec,
        },
    };

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn node_key_getters() {
        let test_node_parent: Node = Parent {
            public_key: b"pub".to_vec().into(),
            parent_hash: ParentHash::empty(),
            unmerged_leaves: vec![],
        }
        .into();

        let test_leaf = get_basic_test_node(TEST_CIPHER_SUITE, "B").await;
        let test_node_leaf: Node = test_leaf.clone().into();

        assert_eq!(test_node_parent.public_key().as_ref(), b"pub");
        assert_eq!(test_node_leaf.public_key(), &test_leaf.public_key);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_empty_leaves() {
        let mut test_vec = get_test_node_vec().await;
        let mut test_vec_clone = get_test_node_vec().await;
        let empty_leaves: Vec<(LeafIndex, &mut Option<Node>)> = test_vec.empty_leaves().collect();
        assert_eq!(
            [(LeafIndex(1), &mut test_vec_clone[2])].as_ref(),
            empty_leaves.as_slice()
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_direct_path() {
        let test_vec = get_test_node_vec().await;
        // Tree math is already tested in that module, just ensure equality
        let expected = 0.direct_copath(&4);
        let actual = test_vec.direct_copath(LeafIndex(0));
        assert_eq!(actual, expected);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_filtered_direct_path_co_path() {
        let test_vec = get_test_node_vec().await;
        let expected = [true, false];
        let actual = test_vec.filtered(LeafIndex(0)).unwrap();
        assert_eq!(actual, expected);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_get_parent_node() {
        let mut test_vec = get_test_node_vec().await;

        // If the node is a leaf it should fail
        assert!(test_vec.borrow_as_parent_mut(0).is_err());

        // If the node index is out of range it should fail
        assert!(test_vec
            .borrow_as_parent_mut(test_vec.len() as u32)
            .is_err());

        // Otherwise it should succeed
        let mut expected = Parent {
            public_key: b"CD".to_vec().into(),
            parent_hash: ParentHash::empty(),
            unmerged_leaves: vec![LeafIndex(2)],
        };

        assert_eq!(test_vec.borrow_as_parent_mut(5).unwrap(), &mut expected);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_get_resolution() {
        let test_vec = get_test_node_vec().await;

        let resolution_node_5 = test_vec.get_resolution_index(5).unwrap();
        let resolution_node_2 = test_vec.get_resolution_index(2).unwrap();
        let resolution_node_3 = test_vec.get_resolution_index(3).unwrap();

        assert_eq!(&resolution_node_5, &[5, 4]);
        assert!(resolution_node_2.is_empty());
        assert_eq!(&resolution_node_3, &[0, 5, 4]);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_get_or_fill_existing() {
        let mut test_vec = get_test_node_vec().await;
        let mut test_vec2 = test_vec.clone();

        let expected = test_vec[5].as_parent_mut().unwrap();
        let actual = test_vec2
            .borrow_or_fill_node_as_parent(5, &Vec::new().into())
            .unwrap();

        assert_eq!(actual, expected);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_get_or_fill_empty() {
        let mut test_vec = get_test_node_vec().await;

        let mut expected = Parent {
            public_key: vec![0u8; 4].into(),
            parent_hash: ParentHash::empty(),
            unmerged_leaves: vec![],
        };

        let actual = test_vec
            .borrow_or_fill_node_as_parent(1, &vec![0u8; 4].into())
            .unwrap();

        assert_eq!(actual, &mut expected);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_leaf_count() {
        let test_vec = get_test_node_vec().await;
        assert_eq!(test_vec.len(), 7);
        assert_eq!(test_vec.occupied_leaf_count(), 3);
        assert_eq!(
            test_vec.non_empty_leaves().count(),
            test_vec.occupied_leaf_count() as usize
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_total_leaf_count() {
        let test_vec = get_test_node_vec().await;
        assert_eq!(test_vec.occupied_leaf_count(), 3);
        assert_eq!(test_vec.total_leaf_count(), 4);
    }
}
