// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::boxed::Box;
use alloc::vec::Vec;

use crate::{
    client::MlsError,
    client_config::ClientConfig,
    group::{
        cipher_suite_provider, epoch::EpochSecrets, key_schedule::KeySchedule,
        message_hash::MessageHash, state_repo::GroupStateRepository, ConfirmationTag, Group,
        GroupContext, GroupState, InterimTranscriptHash, ReInitProposal, TreeKemPublic,
    },
    tree_kem::TreeKemPrivate,
};

#[cfg(feature = "by_ref_proposal")]
use crate::{
    crypto::{HpkePublicKey, HpkeSecretKey},
    group::{
        proposal_cache::{CachedProposal, ProposalCache},
        ProposalMessageDescription, ProposalRef,
    },
    map::SmallMap,
};

use mls_rs_codec::{MlsDecode, MlsEncode, MlsSize};
use mls_rs_core::crypto::SignatureSecretKey;
#[cfg(feature = "tree_index")]
use mls_rs_core::identity::IdentityProvider;

use super::PendingCommit;

pub(crate) use legacy::LegacyPendingCommit;

#[derive(Debug, PartialEq, Clone, MlsEncode, MlsDecode, MlsSize)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct Snapshot {
    version: u16,
    pub(crate) state: RawGroupState,
    private_tree: TreeKemPrivate,
    epoch_secrets: EpochSecrets,
    key_schedule: KeySchedule,
    #[cfg(feature = "by_ref_proposal")]
    pending_updates: SmallMap<HpkePublicKey, (HpkeSecretKey, Option<SignatureSecretKey>)>,
    pending_commit_snapshot: PendingCommitSnapshot,
    signer: SignatureSecretKey,
}

#[derive(Debug, PartialEq, Clone, Default, MlsSize, MlsEncode, MlsDecode)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[repr(u8)]
pub(crate) enum PendingCommitSnapshot {
    #[default]
    None = 0u8,
    // This must be 1 for backwards compatibility
    LegacyPendingCommit(Box<LegacyPendingCommit>) = 1u8,
    PendingCommit(#[mls_codec(with = "mls_rs_codec::byte_vec")] Vec<u8>) = 2u8,
}

impl From<Vec<u8>> for PendingCommitSnapshot {
    fn from(value: Vec<u8>) -> Self {
        Self::PendingCommit(value)
    }
}

impl TryFrom<PendingCommit> for PendingCommitSnapshot {
    type Error = mls_rs_codec::Error;

    fn try_from(value: PendingCommit) -> Result<Self, Self::Error> {
        value.mls_encode_to_vec().map(Self::PendingCommit)
    }
}
impl PendingCommitSnapshot {
    pub fn is_none(&self) -> bool {
        self == &Self::None
    }

    pub fn commit_hash(&self) -> Result<Option<MessageHash>, MlsError> {
        match self {
            Self::None => Ok(None),
            Self::PendingCommit(bytes) => Ok(Some(
                PendingCommit::mls_decode(&mut &bytes[..])?.commit_message_hash,
            )),
            Self::LegacyPendingCommit(legacy_pending) => {
                Ok(Some(legacy_pending.commit_message_hash.clone()))
            }
        }
    }
}

#[derive(Debug, MlsEncode, MlsDecode, MlsSize, PartialEq, Clone)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub(crate) struct RawGroupState {
    pub(crate) context: GroupContext,
    #[cfg(feature = "by_ref_proposal")]
    pub(crate) proposals: SmallMap<ProposalRef, CachedProposal>,
    #[cfg(feature = "by_ref_proposal")]
    pub(crate) own_proposals: SmallMap<MessageHash, ProposalMessageDescription>,
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
            #[cfg(feature = "by_ref_proposal")]
            own_proposals: state.proposals.own_proposals.clone(),
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
            self.own_proposals.clone(),
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
            self.own_proposals.clone(),
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
        self.state_repo.write_to_storage(self.snapshot()?).await
    }

    /// Write the current state of the group to the
    /// [`GroupStorageProvider`](crate::GroupStateStorage)
    /// that is currently in use by the group.
    /// The tree is not included in the state and can be stored
    /// separately by calling [`Group::export_tree`].
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn write_to_storage_without_ratchet_tree(&mut self) -> Result<(), MlsError> {
        let mut snapshot = self.snapshot()?;
        snapshot.state.public_tree.nodes = Default::default();

        self.state_repo.write_to_storage(snapshot).await
    }

    pub(crate) fn snapshot(&self) -> Result<Snapshot, MlsError> {
        Ok(Snapshot {
            state: RawGroupState::export(&self.state),
            private_tree: self.private_tree.clone(),
            key_schedule: self.key_schedule.clone(),
            #[cfg(feature = "by_ref_proposal")]
            pending_updates: self.pending_updates.clone(),
            pending_commit_snapshot: self.pending_commit.clone(),
            epoch_secrets: self.epoch_secrets.clone(),
            version: 1,
            signer: self.signer.clone(),
        })
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
            pending_commit: snapshot.pending_commit_snapshot,
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

mod legacy {
    use crate::{group::AuthenticatedContent, tree_kem::path_secret::PathSecret};

    use super::*;

    #[derive(Clone, PartialEq, Debug, MlsEncode, MlsDecode, MlsSize)]
    #[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
    pub(crate) struct LegacyPendingCommit {
        pub content: AuthenticatedContent,
        pub private_tree: TreeKemPrivate,
        pub commit_secret: PathSecret,
        pub commit_message_hash: MessageHash,
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
                #[cfg(feature = "by_ref_proposal")]
                own_proposals: Default::default(),
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
            pending_commit_snapshot: Default::default(),
            version: 1,
            signer: vec![].into(),
        }
    }
}

#[cfg(test)]
mod tests {
    use alloc::vec;
    use mls_rs_core::group::{GroupState, GroupStateStorage};

    use crate::{
        client::test_utils::{TestClientBuilder, TEST_CIPHER_SUITE, TEST_PROTOCOL_VERSION},
        group::{
            test_utils::{test_group, TestGroup},
            Group,
        },
        storage_provider::in_memory::InMemoryGroupStateStorage,
    };

    #[cfg(all(feature = "std", feature = "by_ref_proposal"))]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn legacy_interop() {
        let mut storage = InMemoryGroupStateStorage::new();

        let legacy_snapshot = include_bytes!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/test_data/legacy_snapshot.mls"
        ));

        let group_state = GroupState {
            id: b"group".into(),
            data: legacy_snapshot.to_vec(),
        };

        storage
            .write(group_state, Default::default(), Default::default())
            .await
            .unwrap();

        let client = TestClientBuilder::new_for_test()
            .group_state_storage(storage)
            .build();

        let mut group = client.load_group(b"group").await.unwrap();

        group
            .apply_pending_commit_backwards_compatible()
            .await
            .unwrap();
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn snapshot_restore(group: TestGroup) {
        let snapshot = group.snapshot().unwrap();

        let group_restored = Group::from_snapshot(group.config.clone(), snapshot)
            .await
            .unwrap();

        assert!(Group::equal_group_state(&group, &group_restored));

        #[cfg(feature = "tree_index")]
        assert!(group_restored
            .state
            .public_tree
            .equal_internals(&group.state.public_tree))
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn snapshot_with_pending_commit_can_be_serialized_to_json() {
        let mut group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;
        group.commit(vec![]).await.unwrap();

        snapshot_restore(group).await
    }

    #[cfg(feature = "by_ref_proposal")]
    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn snapshot_with_pending_updates_can_be_serialized_to_json() {
        let mut group = test_group(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE).await;

        // Creating the update proposal will add it to pending updates
        let update_proposal = group.update_proposal().await;

        // This will insert the proposal into the internal proposal cache
        let _ = group.proposal_message(update_proposal, vec![]).await;

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
