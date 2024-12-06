// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use super::*;
#[cfg(feature = "tree_index")]
use core::fmt::{self, Debug};

#[cfg(all(feature = "tree_index", feature = "custom_proposal"))]
use crate::group::proposal::ProposalType;

#[cfg(feature = "tree_index")]
use crate::identity::CredentialType;

#[cfg(feature = "tree_index")]
use mls_rs_core::crypto::SignaturePublicKey;

#[cfg(all(feature = "tree_index", feature = "std"))]
use itertools::Itertools;

#[cfg(all(feature = "tree_index", not(feature = "std")))]
use alloc::collections::{btree_map::Entry, BTreeMap};

#[cfg(all(feature = "tree_index", feature = "std"))]
use std::collections::{hash_map::Entry, HashMap};

#[cfg(all(feature = "tree_index", not(feature = "std")))]
use alloc::collections::BTreeSet;

#[cfg(feature = "tree_index")]
use mls_rs_core::crypto::HpkePublicKey;

#[cfg(feature = "tree_index")]
#[derive(Clone, Default, PartialEq, Eq, MlsSize, MlsEncode, MlsDecode, Hash, PartialOrd, Ord)]
pub struct Identifier(#[mls_codec(with = "mls_rs_codec::byte_vec")] Vec<u8>);

#[cfg(feature = "tree_index")]
impl Debug for Identifier {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        mls_rs_core::debug::pretty_bytes(&self.0)
            .named("Identifier")
            .fmt(f)
    }
}

#[cfg(all(feature = "tree_index", feature = "std"))]
#[derive(Clone, Debug, Default, PartialEq, MlsSize, MlsEncode, MlsDecode)]
pub struct TreeIndex {
    credential_signature_key: HashMap<SignaturePublicKey, LeafIndex>,
    hpke_key: HashMap<HpkePublicKey, LeafIndex>,
    identities: HashMap<Identifier, LeafIndex>,
    credential_type_counters: HashMap<CredentialType, TypeCounter>,
    #[cfg(feature = "custom_proposal")]
    proposal_type_counter: HashMap<ProposalType, u32>,
}

#[cfg(all(feature = "tree_index", not(feature = "std")))]
#[derive(Clone, Debug, Default, PartialEq, MlsSize, MlsEncode, MlsDecode)]
pub struct TreeIndex {
    credential_signature_key: BTreeMap<SignaturePublicKey, LeafIndex>,
    hpke_key: BTreeMap<HpkePublicKey, LeafIndex>,
    identities: BTreeMap<Identifier, LeafIndex>,
    credential_type_counters: BTreeMap<CredentialType, TypeCounter>,
    #[cfg(feature = "custom_proposal")]
    proposal_type_counter: BTreeMap<ProposalType, u32>,
}

#[cfg(feature = "tree_index")]
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(super) async fn index_insert<I: IdentityProvider>(
    tree_index: &mut TreeIndex,
    new_leaf: &LeafNode,
    new_leaf_idx: LeafIndex,
    id_provider: &I,
    extensions: &ExtensionList,
) -> Result<(), MlsError> {
    let new_id = id_provider
        .identity(&new_leaf.signing_identity, extensions)
        .await
        .map_err(|e| MlsError::IdentityProviderError(e.into_any_error()))?;

    tree_index.insert(new_leaf_idx, new_leaf, new_id)
}

#[cfg(not(feature = "tree_index"))]
#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
pub(super) async fn index_insert<I: IdentityProvider>(
    nodes: &NodeVec,
    new_leaf: &LeafNode,
    new_leaf_idx: LeafIndex,
    id_provider: &I,
    extensions: &ExtensionList,
) -> Result<(), MlsError> {
    let new_id = id_provider
        .identity(&new_leaf.signing_identity, extensions)
        .await
        .map_err(|e| MlsError::IdentityProviderError(e.into_any_error()))?;

    for (i, leaf) in nodes.non_empty_leaves().filter(|(i, _)| i != &new_leaf_idx) {
        (new_leaf.public_key != leaf.public_key)
            .then_some(())
            .ok_or(MlsError::DuplicateLeafData(*i))?;

        (new_leaf.signing_identity.signature_key != leaf.signing_identity.signature_key)
            .then_some(())
            .ok_or(MlsError::DuplicateLeafData(*i))?;

        let id = id_provider
            .identity(&leaf.signing_identity, extensions)
            .await
            .map_err(|e| MlsError::IdentityProviderError(e.into_any_error()))?;

        (new_id != id)
            .then_some(())
            .ok_or(MlsError::DuplicateLeafData(*i))?;

        let cred_type = leaf.signing_identity.credential.credential_type();

        new_leaf
            .capabilities
            .credentials
            .contains(&cred_type)
            .then_some(())
            .ok_or(MlsError::InUseCredentialTypeUnsupportedByNewLeaf)?;

        let new_cred_type = new_leaf.signing_identity.credential.credential_type();

        leaf.capabilities
            .credentials
            .contains(&new_cred_type)
            .then_some(())
            .ok_or(MlsError::CredentialTypeOfNewLeafIsUnsupported)?;
    }

    Ok(())
}

#[cfg(feature = "tree_index")]
impl TreeIndex {
    pub fn new() -> Self {
        Default::default()
    }

    pub fn is_initialized(&self) -> bool {
        !self.identities.is_empty()
    }

    fn insert(
        &mut self,
        index: LeafIndex,
        leaf_node: &LeafNode,
        identity: Vec<u8>,
    ) -> Result<(), MlsError> {
        let old_leaf_count = self.credential_signature_key.len();

        let pub_key = leaf_node.signing_identity.signature_key.clone();
        let credential_entry = self.credential_signature_key.entry(pub_key);

        if let Entry::Occupied(entry) = credential_entry {
            return Err(MlsError::DuplicateLeafData(**entry.get()));
        }

        let hpke_entry = self.hpke_key.entry(leaf_node.public_key.clone());

        if let Entry::Occupied(entry) = hpke_entry {
            return Err(MlsError::DuplicateLeafData(**entry.get()));
        }

        let identity_entry = self.identities.entry(Identifier(identity));
        if let Entry::Occupied(entry) = identity_entry {
            return Err(MlsError::DuplicateLeafData(**entry.get()));
        }

        let in_use_cred_type_unsupported_by_new_leaf = self
            .credential_type_counters
            .iter()
            .filter_map(|(cred_type, counters)| Some(*cred_type).filter(|_| counters.used > 0))
            .find(|cred_type| !leaf_node.capabilities.credentials.contains(cred_type));

        if in_use_cred_type_unsupported_by_new_leaf.is_some() {
            return Err(MlsError::InUseCredentialTypeUnsupportedByNewLeaf);
        }

        let new_leaf_cred_type = leaf_node.signing_identity.credential.credential_type();

        let cred_type_counters = self
            .credential_type_counters
            .entry(new_leaf_cred_type)
            .or_default();

        if cred_type_counters.supported != old_leaf_count as u32 {
            return Err(MlsError::CredentialTypeOfNewLeafIsUnsupported);
        }

        cred_type_counters.used += 1;

        let credential_type_iter = leaf_node.capabilities.credentials.iter().copied();

        #[cfg(feature = "std")]
        let credential_type_iter = credential_type_iter.unique();

        #[cfg(not(feature = "std"))]
        let credential_type_iter = credential_type_iter.collect::<BTreeSet<_>>().into_iter();

        // Credential type counter updates
        credential_type_iter.for_each(|cred_type| {
            self.credential_type_counters
                .entry(cred_type)
                .or_default()
                .supported += 1;
        });

        #[cfg(feature = "custom_proposal")]
        {
            let proposal_type_iter = leaf_node.capabilities.proposals.iter().copied();

            #[cfg(feature = "std")]
            let proposal_type_iter = proposal_type_iter.unique();

            #[cfg(not(feature = "std"))]
            let proposal_type_iter = proposal_type_iter.collect::<BTreeSet<_>>().into_iter();

            // Proposal type counter update
            proposal_type_iter.for_each(|proposal_type| {
                *self.proposal_type_counter.entry(proposal_type).or_default() += 1;
            });
        }

        identity_entry.or_insert(index);
        credential_entry.or_insert(index);
        hpke_entry.or_insert(index);

        Ok(())
    }

    pub(crate) fn get_leaf_index_with_identity(&self, identity: &[u8]) -> Option<LeafIndex> {
        self.identities.get(&Identifier(identity.to_vec())).copied()
    }

    pub fn remove(&mut self, leaf_node: &LeafNode, identity: &[u8]) {
        let existed = self
            .identities
            .remove(&Identifier(identity.to_vec()))
            .is_some();

        self.credential_signature_key
            .remove(&leaf_node.signing_identity.signature_key);

        self.hpke_key.remove(&leaf_node.public_key);

        if !existed {
            return;
        }

        // Decrement credential type counters
        let leaf_cred_type = leaf_node.signing_identity.credential.credential_type();

        if let Some(counters) = self.credential_type_counters.get_mut(&leaf_cred_type) {
            counters.used -= 1;
        }

        let credential_type_iter = leaf_node.capabilities.credentials.iter();

        #[cfg(feature = "std")]
        let credential_type_iter = credential_type_iter.unique();

        #[cfg(not(feature = "std"))]
        let credential_type_iter = credential_type_iter.collect::<BTreeSet<_>>().into_iter();

        credential_type_iter.for_each(|cred_type| {
            if let Some(counters) = self.credential_type_counters.get_mut(cred_type) {
                counters.supported -= 1;
            }
        });

        #[cfg(feature = "custom_proposal")]
        {
            let proposal_type_iter = leaf_node.capabilities.proposals.iter();

            #[cfg(feature = "std")]
            let proposal_type_iter = proposal_type_iter.unique();

            #[cfg(not(feature = "std"))]
            let proposal_type_iter = proposal_type_iter.collect::<BTreeSet<_>>().into_iter();

            // Decrement proposal type counters
            proposal_type_iter.for_each(|proposal_type| {
                if let Some(supported) = self.proposal_type_counter.get_mut(proposal_type) {
                    *supported -= 1;
                }
            })
        }
    }

    #[cfg(feature = "custom_proposal")]
    pub fn count_supporting_proposal(&self, proposal_type: ProposalType) -> u32 {
        self.proposal_type_counter
            .get(&proposal_type)
            .copied()
            .unwrap_or_default()
    }

    #[cfg(test)]
    pub fn len(&self) -> usize {
        self.credential_signature_key.len()
    }
}

#[cfg(feature = "tree_index")]
#[derive(Clone, Debug, Default, PartialEq, MlsEncode, MlsDecode, MlsSize)]
struct TypeCounter {
    supported: u32,
    used: u32,
}

#[cfg(feature = "tree_index")]
#[cfg(test)]
mod tests {
    use super::*;
    use crate::{
        client::test_utils::TEST_CIPHER_SUITE,
        tree_kem::leaf_node::test_utils::{get_basic_test_node, get_test_client_identity},
    };
    use alloc::format;
    use assert_matches::assert_matches;

    #[derive(Clone, Debug)]
    struct TestData {
        pub leaf_node: LeafNode,
        pub index: LeafIndex,
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn get_test_data(index: LeafIndex) -> TestData {
        let cipher_suite = TEST_CIPHER_SUITE;
        let leaf_node = get_basic_test_node(cipher_suite, &format!("foo{}", index.0)).await;

        TestData { leaf_node, index }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_setup() -> (Vec<TestData>, TreeIndex) {
        let mut test_data = Vec::new();

        for i in 0..10 {
            test_data.push(get_test_data(LeafIndex(i)).await);
        }

        let mut test_index = TreeIndex::new();

        test_data.clone().into_iter().for_each(|d| {
            test_index
                .insert(
                    d.index,
                    &d.leaf_node,
                    get_test_client_identity(&d.leaf_node),
                )
                .unwrap()
        });

        (test_data, test_index)
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_insert() {
        let (test_data, test_index) = test_setup().await;

        assert_eq!(test_index.credential_signature_key.len(), test_data.len());
        assert_eq!(test_index.hpke_key.len(), test_data.len());

        test_data.into_iter().enumerate().for_each(|(i, d)| {
            let pub_key = d.leaf_node.signing_identity.signature_key;

            assert_eq!(
                test_index.credential_signature_key.get(&pub_key),
                Some(&LeafIndex(i as u32))
            );

            assert_eq!(
                test_index.hpke_key.get(&d.leaf_node.public_key),
                Some(&LeafIndex(i as u32))
            );
        })
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_insert_duplicate_credential_key() {
        let (test_data, mut test_index) = test_setup().await;

        let before_error = test_index.clone();

        let mut new_key_package = get_basic_test_node(TEST_CIPHER_SUITE, "foo").await;
        new_key_package.signing_identity = test_data[1].leaf_node.signing_identity.clone();

        let res = test_index.insert(
            test_data[1].index,
            &new_key_package,
            get_test_client_identity(&new_key_package),
        );

        assert_matches!(res, Err(MlsError::DuplicateLeafData(index))
                        if index == *test_data[1].index);

        assert_eq!(before_error, test_index);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_insert_duplicate_hpke_key() {
        let cipher_suite = TEST_CIPHER_SUITE;
        let (test_data, mut test_index) = test_setup().await;
        let before_error = test_index.clone();

        let mut new_leaf_node = get_basic_test_node(cipher_suite, "foo").await;
        new_leaf_node.public_key = test_data[1].leaf_node.public_key.clone();

        let res = test_index.insert(
            test_data[1].index,
            &new_leaf_node,
            get_test_client_identity(&new_leaf_node),
        );

        assert_matches!(res, Err(MlsError::DuplicateLeafData(index))
                        if index == *test_data[1].index);

        assert_eq!(before_error, test_index);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_remove() {
        let (test_data, mut test_index) = test_setup().await;

        test_index.remove(
            &test_data[1].leaf_node,
            &get_test_client_identity(&test_data[1].leaf_node),
        );

        assert_eq!(
            test_index.credential_signature_key.len(),
            test_data.len() - 1
        );

        assert_eq!(test_index.hpke_key.len(), test_data.len() - 1);

        assert_eq!(
            test_index
                .credential_signature_key
                .get(&test_data[1].leaf_node.signing_identity.signature_key),
            None
        );

        assert_eq!(
            test_index.hpke_key.get(&test_data[1].leaf_node.public_key),
            None
        );
    }

    #[cfg(feature = "custom_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn custom_proposals() {
        let test_proposal_id = ProposalType::new(42);
        let other_proposal_id = ProposalType::new(45);

        let mut test_data_1 = get_test_data(LeafIndex(0)).await;

        test_data_1
            .leaf_node
            .capabilities
            .proposals
            .push(test_proposal_id);

        let mut test_data_2 = get_test_data(LeafIndex(1)).await;

        test_data_2
            .leaf_node
            .capabilities
            .proposals
            .push(test_proposal_id);

        test_data_2
            .leaf_node
            .capabilities
            .proposals
            .push(other_proposal_id);

        let mut test_index = TreeIndex::new();

        test_index
            .insert(test_data_1.index, &test_data_1.leaf_node, vec![0])
            .unwrap();

        assert_eq!(test_index.count_supporting_proposal(test_proposal_id), 1);

        test_index
            .insert(test_data_2.index, &test_data_2.leaf_node, vec![1])
            .unwrap();

        assert_eq!(test_index.count_supporting_proposal(test_proposal_id), 2);
        assert_eq!(test_index.count_supporting_proposal(other_proposal_id), 1);

        test_index.remove(&test_data_2.leaf_node, &[1]);

        assert_eq!(test_index.count_supporting_proposal(test_proposal_id), 1);
        assert_eq!(test_index.count_supporting_proposal(other_proposal_id), 0);
    }
}
