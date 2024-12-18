// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::{
    client::MlsError,
    client_config::ClientConfig,
    group::{
        key_schedule::KeySchedule, CommitGeneration, ConfirmationTag, Group, GroupContext,
        GroupState, InterimTranscriptHash, ReInitProposal, TreeKemPublic,
    },
    tree_kem::TreeKemPrivate,
};

#[cfg(feature = "by_ref_proposal")]
use crate::{
    crypto::{HpkePublicKey, HpkeSecretKey},
    group::ProposalRef,
};

#[cfg(feature = "by_ref_proposal")]
use super::proposal_cache::{CachedProposal, ProposalCache};

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};

use mls_rs_core::crypto::SignatureSecretKey;
#[cfg(feature = "tree_index")]
use mls_rs_core::identity::IdentityProvider;

#[cfg(all(feature = "std", feature = "by_ref_proposal"))]
use std::collections::HashMap;

#[cfg(all(feature = "by_ref_proposal", not(feature = "std")))]
use alloc::vec::Vec;

use super::{cipher_suite_provider, epoch::EpochSecrets, state_repo::GroupStateRepository};

#[derive(Debug, PartialEq, Clone, MlsEncode, MlsDecode, MlsSize)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct Snapshot {
    version: u16,
    pub(crate) state: RawGroupState,
    private_tree: TreeKemPrivate,
    epoch_secrets: EpochSecrets,
    key_schedule: KeySchedule,
    #[cfg(all(feature = "std", feature = "by_ref_proposal"))]
    pending_updates: HashMap<HpkePublicKey, (HpkeSecretKey, Option<SignatureSecretKey>)>,
    #[cfg(all(not(feature = "std"), feature = "by_ref_proposal"))]
    pending_updates: Vec<(HpkePublicKey, (HpkeSecretKey, Option<SignatureSecretKey>))>,
    pending_commit: Option<CommitGeneration>,
    signer: SignatureSecretKey,
}

#[derive(Debug, MlsEncode, MlsDecode, MlsSize, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct RawGroupState {
    pub(crate) context: GroupContext,
    #[cfg(all(feature = "std", feature = "by_ref_proposal"))]
    pub(crate) proposals: HashMap<ProposalRef, CachedProposal>,
    #[cfg(all(not(feature = "std"), feature = "by_ref_proposal"))]
    pub(crate) proposals: Vec<(ProposalRef, CachedProposal)>,
    pub(crate) public_tree: TreeKemPublic,
    pub(crate) interim_transcript_hash: InterimTranscriptHash,
    pub(crate) pending_reinit: Option<ReInitProposal>,
    pub(crate) confirmation_tag: ConfirmationTag,
}

impl RawGroupState {
    pub(crate) fn export(state: &GroupState) -> Self {
        #[cfg(feature = "tree_index")]
        let public_tree = state.public_tree.clone();

        #[cfg(not(feature = "tree_index"))]
        let public_tree = {
            let mut tree = TreeKemPublic::new();
            tree.nodes = state.public_tree.nodes.clone();
            tree
        };

        Self {
            context: state.context.clone(),
            #[cfg(feature = "by_ref_proposal")]
            proposals: state.proposals.proposals.clone(),
            public_tree,
            interim_transcript_hash: state.interim_transcript_hash.clone(),
            pending_reinit: state.pending_reinit.clone(),
            confirmation_tag: state.confirmation_tag.clone(),
        }
    }

    #[cfg(feature = "tree_index")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn import<C>(self, identity_provider: &C) -> Result<GroupState, MlsError>
    where
        C: IdentityProvider,
    {
        let context = self.context;

        #[cfg(feature = "by_ref_proposal")]
        let proposals = ProposalCache::import(
            context.protocol_version,
            context.group_id.clone(),
            self.proposals,
        );

        let mut public_tree = self.public_tree;

        public_tree
            .initialize_index_if_necessary(identity_provider, &context.extensions)
            .await?;

        Ok(GroupState {
            #[cfg(feature = "by_ref_proposal")]
            proposals,
            context,
            public_tree,
            interim_transcript_hash: self.interim_transcript_hash,
            pending_reinit: self.pending_reinit,
            confirmation_tag: self.confirmation_tag,
        })
    }

    #[cfg(not(feature = "tree_index"))]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn import(self) -> Result<GroupState, MlsError> {
        let context = self.context;

        #[cfg(feature = "by_ref_proposal")]
        let proposals = ProposalCache::import(
            context.protocol_version,
            context.group_id.clone(),
            self.proposals,
        );

        Ok(GroupState {
            #[cfg(feature = "by_ref_proposal")]
            proposals,
            context,
            public_tree: self.public_tree,
            interim_transcript_hash: self.interim_transcript_hash,
            pending_reinit: self.pending_reinit,
            confirmation_tag: self.confirmation_tag,
        })
    }
}

impl<C> Group<C>
where
    C: ClientConfig + Clone,
{
    /// Write the current state of the group to the
    /// [`GroupStorageProvider`](crate::GroupStateStorage)
    /// that is currently in use by the group.
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn write_to_storage(&mut self) -> Result<(), MlsError> {
        self.state_repo.write_to_storage(self.snapshot()).await
    }

    pub(crate) fn snapshot(&self) -> Snapshot {
        Snapshot {
            state: RawGroupState::export(&self.state),
            private_tree: self.private_tree.clone(),
            key_schedule: self.key_schedule.clone(),
            #[cfg(feature = "by_ref_proposal")]
            pending_updates: self.pending_updates.clone(),
            pending_commit: self.pending_commit.clone(),
            epoch_secrets: self.epoch_secrets.clone(),
            version: 1,
            signer: self.signer.clone(),
        }
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn from_snapshot(config: C, snapshot: Snapshot) -> Result<Self, MlsError> {
        let cipher_suite_provider = cipher_suite_provider(
            config.crypto_provider(),
            snapshot.state.context.cipher_suite,
        )?;

        #[cfg(feature = "tree_index")]
        let identity_provider = config.identity_provider();

        let state_repo = GroupStateRepository::new(
            #[cfg(feature = "prior_epoch")]
            snapshot.state.context.group_id.clone(),
            config.group_state_storage(),
            config.key_package_repo(),
            None,
        )?;

        Ok(Group {
            config,
            state: snapshot
                .state
                .import(
                    #[cfg(feature = "tree_index")]
                    &identity_provider,
                )
                .await?,
            private_tree: snapshot.private_tree,
            key_schedule: snapshot.key_schedule,
            #[cfg(feature = "by_ref_proposal")]
            pending_updates: snapshot.pending_updates,
            pending_commit: snapshot.pending_commit,
            #[cfg(test)]
            commit_modifiers: Default::default(),
            epoch_secrets: snapshot.epoch_secrets,
            state_repo,
            cipher_suite_provider,
            #[cfg(feature = "psk")]
            previous_psk: None,
            signer: snapshot.signer,
        })
    }
}

#[cfg(test)]
pub(crate) mod test_utils {
    use alloc::vec;

    use crate::{
        cipher_suite::CipherSuite,
        crypto::test_utils::test_cipher_suite_provider,
        group::{
            confirmation_tag::ConfirmationTag, epoch::test_utils::get_test_epoch_secrets,
            key_schedule::test_utils::get_test_key_schedule, test_utils::get_test_group_context,
            transcript_hash::InterimTranscriptHash,
        },
        tree_kem::{node::LeafIndex, TreeKemPrivate},
    };

    use super::{RawGroupState, Snapshot};

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub(crate) async fn get_test_snapshot(cipher_suite: CipherSuite, epoch_id: u64) -> Snapshot {
        Snapshot {
            state: RawGroupState {
                context: get_test_group_context(epoch_id, cipher_suite).await,
                #[cfg(feature = "by_ref_proposal")]
                proposals: Default::default(),
                public_tree: Default::default(),
                interim_transcript_hash: InterimTranscriptHash::from(vec![]),
                pending_reinit: None,
                confirmation_tag: ConfirmationTag::empty(&test_cipher_suite_provider(cipher_suite))
                    .await,
            },
            private_tree: TreeKemPrivate::new(LeafIndex(0)),
            epoch_secrets: get_test_epoch_secrets(cipher_suite),
            key_schedule: get_test_key_schedule(cipher_suite),
            #[cfg(feature = "by_ref_proposal")]
            pending_updates: Default::default(),
            pending_commit: None,
            version: 1,
            signer: vec![].into(),
        }
    }
}

#[cfg(test)]
mod tests {
    use alloc::vec;

    use crate::{
        client::test_utils::{TEST_CIPHER_SUITE, TEST_PROTOCOL_VERSION},
        group::{
            test_utils::{test_group, TestGroup},
            Group,
        },
    };

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn snapshot_restore(group: TestGroup) {
        let snapshot = group.group.snapshot();

        let group_restored = Group::from_snapshot(group.group.config.clone(), snapshot)
            .await
            .unwrap();

        assert!(Group::equal_group_state(&group.group, &group_restored));

        #[cfg(feature = "tree_index")]
        assert!(group_restored
            .state
            .public_tree
            .equal_internals(&group.group.state.public_tree))
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn snapshot_with_pending_commit_can_be_serialized_to_json() {
        let mut group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        group.group.commit(vec![]).await.unwrap();

        snapshot_restore(group).await
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn snapshot_with_pending_updates_can_be_serialized_to_json() {
        let mut group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        // Creating the update proposal will add it to pending updates
        let update_proposal = group.update_proposal().await;

        // This will insert the proposal into the internal proposal cache
        let _ = group.group.proposal_message(update_proposal, vec![]).await;

        snapshot_restore(group).await
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn snapshot_can_be_serialized_to_json_with_internals() {
        let group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        snapshot_restore(group).await
    }

    #[cfg(feature = "serde")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn serde() {
        let snapshot = super::test_utils::get_test_snapshot(TEST_CIPHER_SUITE, 5).await;
        let json = serde_json::to_string_pretty(&snapshot).unwrap();
        let recovered = serde_json::from_str(&json).unwrap();
        assert_eq!(snapshot, recovered);
    }
}
