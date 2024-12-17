// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use super::*;

pub use mls_rs_core::group::Member;

#[cfg(feature = "state_update")]
pub(crate) fn member_from_key_package(key_package: &KeyPackage, index: LeafIndex) -> Member {
    member_from_leaf_node(&key_package.leaf_node, index)
}

pub(crate) fn member_from_leaf_node(leaf_node: &LeafNode, leaf_index: LeafIndex) -> Member {
    Member::new(
        *leaf_index,
        leaf_node.signing_identity.clone(),
        leaf_node.ungreased_capabilities(),
        leaf_node.ungreased_extensions(),
    )
}

// #[cfg_attr(
//     all(feature = "ffi", not(test)),
//     safer_ffi_gen::ffi_type(clone, opaque)
// )]
#[derive(Clone, Debug)]
pub struct Roster<'a> {
    pub(crate) public_tree: &'a TreeKemPublic,
}

// #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen)]
impl<'a> Roster<'a> {
    /// Iterator over the current roster that lazily copies data out of the
    /// internal group state.
    ///
    /// # Warning
    ///
    /// The indexes within this iterator do not correlate with indexes of users
    /// within [`ReceivedMessage`] content descriptions due to the layout of
    /// member information within a MLS group state.
    // #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen_ignore)]
    pub fn members_iter(&self) -> impl Iterator<Item = Member> + 'a {
        self.public_tree
            .non_empty_leaves()
            .map(|(index, node)| member_from_leaf_node(node, index))
    }

    /// The current set of group members. This function makes a clone of
    /// member information from the internal group state.
    ///
    /// # Warning
    ///
    /// The indexes within this roster do not correlate with indexes of users
    /// within [`ReceivedMessage`] content descriptions due to the layout of
    /// member information within a MLS group state.
    pub fn members(&self) -> Vec<Member> {
        self.members_iter().collect()
    }

    /// Retrieve the member with given `index` within the group in time `O(1)`.
    /// This index does correlate with indexes of users within [`ReceivedMessage`]
    /// content descriptions.
    pub fn member_with_index(&self, index: u32) -> Result<Member, MlsError> {
        let index = LeafIndex(index);

        self.public_tree
            .get_leaf_node(index)
            .map(|l| member_from_leaf_node(l, index))
    }

    /// Iterator over member's signing identities.
    ///
    /// # Warning
    ///
    /// The indexes within this iterator do not correlate with indexes of users
    /// within [`ReceivedMessage`] content descriptions due to the layout of
    /// member information within a MLS group state.
    // #[cfg_attr(all(feature = "ffi", not(test)), safer_ffi_gen::safer_ffi_gen_ignore)]
    pub fn member_identities_iter(&self) -> impl Iterator<Item = &SigningIdentity> + '_ {
        self.public_tree
            .non_empty_leaves()
            .map(|(_, node)| &node.signing_identity)
    }
}

impl TreeKemPublic {
    pub(crate) fn roster(&self) -> Roster {
        Roster { public_tree: self }
    }
}
