// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::client::MlsError;
use crate::{group::PriorEpoch, key_package::KeyPackageRef};

use alloc::collections::VecDeque;
use alloc::vec::Vec;
use core::fmt::{self, Debug};
use mls_rs_codec::{MlsDecode, MlsEncode};
use mls_rs_core::group::{EpochRecord, GroupState};
use mls_rs_core::{error::IntoAnyError, group::GroupStateStorage, key_package::KeyPackageStorage};

use super::snapshot::Snapshot;

#[cfg(feature = "psk")]
use crate::group::ResumptionPsk;

#[cfg(feature = "psk")]
use mls_rs_core::psk::PreSharedKey;

/// A set of changes to apply to a GroupStateStorage implementation. These changes MUST
/// be made in a single transaction to avoid creating invalid states.
#[derive(Default, Clone, Debug)]
struct EpochStorageCommit {
    pub(crate) inserts: VecDeque<PriorEpoch>,
    pub(crate) updates: Vec<PriorEpoch>,
}

#[derive(Clone)]
pub(crate) struct GroupStateRepository<S, K>
where
    S: GroupStateStorage,
    K: KeyPackageStorage,
{
    pending_commit: EpochStorageCommit,
    pending_key_package_removal: Option<KeyPackageRef>,
    group_id: Vec<u8>,
    storage: S,
    key_package_repo: K,
}

impl<S, K> Debug for GroupStateRepository<S, K>
where
    S: GroupStateStorage + Debug,
    K: KeyPackageStorage + Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("GroupStateRepository")
            .field("pending_commit", &self.pending_commit)
            .field(
                "pending_key_package_removal",
                &self.pending_key_package_removal,
            )
            .field(
                "group_id",
                &mls_rs_core::debug::pretty_group_id(&self.group_id),
            )
            .field("storage", &self.storage)
            .field("key_package_repo", &self.key_package_repo)
            .finish()
    }
}

impl<S, K> GroupStateRepository<S, K>
where
    S: GroupStateStorage,
    K: KeyPackageStorage,
{
    pub fn new(
        group_id: Vec<u8>,
        storage: S,
        key_package_repo: K,
        // Set to `None` if restoring from snapshot; set to `Some` when joining a group.
        key_package_to_remove: Option<KeyPackageRef>,
    ) -> Result<GroupStateRepository<S, K>, MlsError> {
        Ok(GroupStateRepository {
            group_id,
            storage,
            pending_key_package_removal: key_package_to_remove,
            pending_commit: Default::default(),
            key_package_repo,
        })
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn find_max_id(&self) -> Result<Option<u64>, MlsError> {
        if let Some(max) = self.pending_commit.inserts.back().map(|e| e.epoch_id()) {
            Ok(Some(max))
        } else {
            self.storage
                .max_epoch_id(&self.group_id)
                .await
                .map_err(|e| MlsError::GroupStorageError(e.into_any_error()))
        }
    }

    #[cfg(feature = "psk")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn resumption_secret(
        &self,
        psk_id: &ResumptionPsk,
    ) -> Result<Option<PreSharedKey>, MlsError> {
        // Search the local inserts cache
        if let Some(min) = self.pending_commit.inserts.front().map(|e| e.epoch_id()) {
            if psk_id.psk_epoch >= min {
                return Ok(self
                    .pending_commit
                    .inserts
                    .get((psk_id.psk_epoch - min) as usize)
                    .map(|e| e.secrets.resumption_secret.clone()));
            }
        }

        // Search the local updates cache
        let maybe_pending = self.find_pending(psk_id.psk_epoch);

        if let Some(pending) = maybe_pending {
            return Ok(Some(
                self.pending_commit.updates[pending]
                    .secrets
                    .resumption_secret
                    .clone(),
            ));
        }

        // Search the stored cache
        self.storage
            .epoch(&psk_id.psk_group_id.0, psk_id.psk_epoch)
            .await
            .map_err(|e| MlsError::GroupStorageError(e.into_any_error()))?
            .map(|e| Ok(PriorEpoch::mls_decode(&mut &*e)?.secrets.resumption_secret))
            .transpose()
    }

    #[cfg(feature = "private_message")]
    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn get_epoch_mut(
        &mut self,
        epoch_id: u64,
    ) -> Result<Option<&mut PriorEpoch>, MlsError> {
        // Search the local inserts cache
        if let Some(min) = self.pending_commit.inserts.front().map(|e| e.epoch_id()) {
            if epoch_id >= min {
                return Ok(self
                    .pending_commit
                    .inserts
                    .get_mut((epoch_id - min) as usize));
            }
        }

        // Look in the cached updates map, and if not found look in disk storage
        // and insert into the updates map for future caching
        match self.find_pending(epoch_id) {
            Some(i) => self.pending_commit.updates.get_mut(i).map(Ok),
            None => self
                .storage
                .epoch(&self.group_id, epoch_id)
                .await
                .map_err(|e| MlsError::GroupStorageError(e.into_any_error()))?
                .and_then(|epoch| {
                    PriorEpoch::mls_decode(&mut &*epoch)
                        .map(|epoch| {
                            self.pending_commit.updates.push(epoch);
                            self.pending_commit.updates.last_mut()
                        })
                        .transpose()
                }),
        }
        .transpose()
        .map_err(Into::into)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn insert(&mut self, epoch: PriorEpoch) -> Result<(), MlsError> {
        if epoch.group_id() != self.group_id {
            return Err(MlsError::GroupIdMismatch);
        }

        let epoch_id = epoch.epoch_id();

        if let Some(expected_id) = self.find_max_id().await?.map(|id| id + 1) {
            if epoch_id != expected_id {
                return Err(MlsError::InvalidEpoch);
            }
        }

        self.pending_commit.inserts.push_back(epoch);

        Ok(())
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn write_to_storage(&mut self, group_snapshot: Snapshot) -> Result<(), MlsError> {
        let inserts = self
            .pending_commit
            .inserts
            .iter()
            .map(|e| Ok(EpochRecord::new(e.epoch_id(), e.mls_encode_to_vec()?)))
            .collect::<Result<_, MlsError>>()?;

        let updates = self
            .pending_commit
            .updates
            .iter()
            .map(|e| Ok(EpochRecord::new(e.epoch_id(), e.mls_encode_to_vec()?)))
            .collect::<Result<_, MlsError>>()?;

        let group_state = GroupState {
            data: group_snapshot.mls_encode_to_vec()?,
            id: group_snapshot.state.context.group_id,
        };

        self.storage
            .write(group_state, inserts, updates)
            .await
            .map_err(|e| MlsError::GroupStorageError(e.into_any_error()))?;

        if let Some(ref key_package_ref) = self.pending_key_package_removal {
            self.key_package_repo
                .delete(key_package_ref)
                .await
                .map_err(|e| MlsError::KeyPackageRepoError(e.into_any_error()))?;
        }

        self.pending_commit.inserts.clear();
        self.pending_commit.updates.clear();

        Ok(())
    }

    #[cfg(any(feature = "psk", feature = "private_message"))]
    fn find_pending(&self, epoch_id: u64) -> Option<usize> {
        self.pending_commit
            .updates
            .iter()
            .position(|ep| ep.context.epoch == epoch_id)
    }
}

#[cfg(test)]
mod tests {
    use alloc::vec;
    use mls_rs_codec::MlsEncode;

    use crate::{
        client::test_utils::{TEST_CIPHER_SUITE, TEST_PROTOCOL_VERSION},
        group::{
            epoch::{test_utils::get_test_epoch_with_id, SenderDataSecret},
            test_utils::{random_bytes, test_member, TEST_GROUP},
            PskGroupId, ResumptionPSKUsage,
        },
        storage_provider::in_memory::{InMemoryGroupStateStorage, InMemoryKeyPackageStorage},
    };

    use super::*;

    fn test_group_state_repo(
        retention_limit: usize,
    ) -> GroupStateRepository<InMemoryGroupStateStorage, InMemoryKeyPackageStorage> {
        GroupStateRepository::new(
            TEST_GROUP.to_vec(),
            InMemoryGroupStateStorage::new()
                .with_max_epoch_retention(retention_limit)
                .unwrap(),
            InMemoryKeyPackageStorage::default(),
            None,
        )
        .unwrap()
    }

    fn test_epoch(epoch_id: u64) -> PriorEpoch {
        get_test_epoch_with_id(TEST_GROUP.to_vec(), TEST_CIPHER_SUITE, epoch_id)
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_snapshot(epoch_id: u64) -> Snapshot {
        crate::group::snapshot::test_utils::get_test_snapshot(TEST_CIPHER_SUITE, epoch_id).await
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_epoch_inserts() {
        let mut test_repo = test_group_state_repo(1);
        let test_epoch = test_epoch(0);

        test_repo.insert(test_epoch.clone()).await.unwrap();

        // Check the in-memory state
        assert_eq!(
            test_repo.pending_commit.inserts.back().unwrap(),
            &test_epoch
        );

        assert!(test_repo.pending_commit.updates.is_empty());

        #[cfg(feature = "std")]
        assert!(test_repo.storage.inner.lock().unwrap().is_empty());
        #[cfg(not(feature = "std"))]
        assert!(test_repo.storage.inner.lock().is_empty());

        let psk_id = ResumptionPsk {
            psk_epoch: 0,
            psk_group_id: PskGroupId(test_repo.group_id.clone()),
            usage: ResumptionPSKUsage::Application,
        };

        // Make sure you can recall an epoch sitting as a pending insert
        let resumption = test_repo.resumption_secret(&psk_id).await.unwrap();
        let prior_epoch = test_repo.get_epoch_mut(0).await.unwrap().cloned();

        assert_eq!(
            prior_epoch.clone().unwrap().secrets.resumption_secret,
            resumption.unwrap()
        );

        assert_eq!(prior_epoch.unwrap(), test_epoch);

        // Write to the storage
        let snapshot = test_snapshot(test_epoch.epoch_id()).await;
        test_repo.write_to_storage(snapshot.clone()).await.unwrap();

        // Make sure the memory cache cleared
        assert!(test_repo.pending_commit.inserts.is_empty());
        assert!(test_repo.pending_commit.updates.is_empty());

        // Make sure the storage was written
        #[cfg(feature = "std")]
        let storage = test_repo.storage.inner.lock().unwrap();
        #[cfg(not(feature = "std"))]
        let storage = test_repo.storage.inner.lock();

        assert_eq!(storage.len(), 1);

        let stored = storage.get(TEST_GROUP).unwrap();

        assert_eq!(stored.state_data, snapshot.mls_encode_to_vec().unwrap());

        assert_eq!(stored.epoch_data.len(), 1);

        assert_eq!(
            stored.epoch_data.back().unwrap(),
            &EpochRecord::new(
                test_epoch.epoch_id(),
                test_epoch.mls_encode_to_vec().unwrap()
            )
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_updates() {
        let mut test_repo = test_group_state_repo(2);
        let test_epoch_0 = test_epoch(0);

        test_repo.insert(test_epoch_0.clone()).await.unwrap();

        test_repo
            .write_to_storage(test_snapshot(0).await)
            .await
            .unwrap();

        // Update the stored epoch
        let to_update = test_repo.get_epoch_mut(0).await.unwrap().unwrap();
        assert_eq!(to_update, &test_epoch_0);

        let new_sender_secret = random_bytes(32);
        to_update.secrets.sender_data_secret = SenderDataSecret::from(new_sender_secret);
        let to_update = to_update.clone();

        assert_eq!(test_repo.pending_commit.updates.len(), 1);
        assert!(test_repo.pending_commit.inserts.is_empty());

        assert_eq!(
            test_repo.pending_commit.updates.first().unwrap(),
            &to_update
        );

        // Make sure you can access an epoch pending update
        let psk_id = ResumptionPsk {
            psk_epoch: 0,
            psk_group_id: PskGroupId(test_repo.group_id.clone()),
            usage: ResumptionPSKUsage::Application,
        };

        let owned = test_repo.resumption_secret(&psk_id).await.unwrap();
        assert_eq!(owned.as_ref(), Some(&to_update.secrets.resumption_secret));

        // Write the update to storage
        let snapshot = test_snapshot(1).await;
        test_repo.write_to_storage(snapshot.clone()).await.unwrap();

        assert!(test_repo.pending_commit.updates.is_empty());
        assert!(test_repo.pending_commit.inserts.is_empty());

        // Make sure the storage was written
        #[cfg(feature = "std")]
        let storage = test_repo.storage.inner.lock().unwrap();
        #[cfg(not(feature = "std"))]
        let storage = test_repo.storage.inner.lock();

        assert_eq!(storage.len(), 1);

        let stored = storage.get(TEST_GROUP).unwrap();

        assert_eq!(stored.state_data, snapshot.mls_encode_to_vec().unwrap());

        assert_eq!(stored.epoch_data.len(), 1);

        assert_eq!(
            stored.epoch_data.back().unwrap(),
            &EpochRecord::new(to_update.epoch_id(), to_update.mls_encode_to_vec().unwrap())
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_insert_and_update() {
        let mut test_repo = test_group_state_repo(2);
        let test_epoch_0 = test_epoch(0);

        test_repo.insert(test_epoch_0).await.unwrap();

        test_repo
            .write_to_storage(test_snapshot(0).await)
            .await
            .unwrap();

        // Update the stored epoch
        let to_update = test_repo.get_epoch_mut(0).await.unwrap().unwrap();
        let new_sender_secret = random_bytes(32);
        to_update.secrets.sender_data_secret = SenderDataSecret::from(new_sender_secret);
        let to_update = to_update.clone();

        // Insert another epoch
        let test_epoch_1 = test_epoch(1);
        test_repo.insert(test_epoch_1.clone()).await.unwrap();

        test_repo
            .write_to_storage(test_snapshot(1).await)
            .await
            .unwrap();

        assert!(test_repo.pending_commit.inserts.is_empty());
        assert!(test_repo.pending_commit.updates.is_empty());

        // Make sure the storage was written
        #[cfg(feature = "std")]
        let storage = test_repo.storage.inner.lock().unwrap();
        #[cfg(not(feature = "std"))]
        let storage = test_repo.storage.inner.lock();

        assert_eq!(storage.len(), 1);

        let stored = storage.get(TEST_GROUP).unwrap();

        assert_eq!(stored.epoch_data.len(), 2);

        assert_eq!(
            stored.epoch_data.front().unwrap(),
            &EpochRecord::new(to_update.epoch_id(), to_update.mls_encode_to_vec().unwrap())
        );

        assert_eq!(
            stored.epoch_data.back().unwrap(),
            &EpochRecord::new(
                test_epoch_1.epoch_id(),
                test_epoch_1.mls_encode_to_vec().unwrap()
            )
        );
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_many_epochs_in_storage() {
        let epochs = (0..10).map(test_epoch).collect::<Vec<_>>();

        let mut test_repo = test_group_state_repo(10);

        for epoch in epochs.iter().cloned() {
            test_repo.insert(epoch).await.unwrap()
        }

        test_repo
            .write_to_storage(test_snapshot(9).await)
            .await
            .unwrap();

        for mut epoch in epochs {
            let res = test_repo.get_epoch_mut(epoch.epoch_id()).await.unwrap();

            assert_eq!(res, Some(&mut epoch));
        }
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_stored_groups_list() {
        let mut test_repo = test_group_state_repo(2);
        let test_epoch_0 = test_epoch(0);

        test_repo.insert(test_epoch_0.clone()).await.unwrap();

        test_repo
            .write_to_storage(test_snapshot(0).await)
            .await
            .unwrap();

        assert_eq!(
            test_repo.storage.stored_groups(),
            vec![test_epoch_0.context.group_id]
        )
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn reducing_retention_limit_takes_effect_on_epoch_access() {
        let mut repo = test_group_state_repo(1);

        repo.insert(test_epoch(0)).await.unwrap();
        repo.insert(test_epoch(1)).await.unwrap();

        repo.write_to_storage(test_snapshot(0).await).await.unwrap();

        let mut repo = GroupStateRepository {
            storage: repo.storage,
            ..test_group_state_repo(1)
        };

        let res = repo.get_epoch_mut(0).await.unwrap();

        assert!(res.is_none());
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn in_memory_storage_obeys_retention_limit_after_saving() {
        let mut repo = test_group_state_repo(1);

        repo.insert(test_epoch(0)).await.unwrap();
        repo.write_to_storage(test_snapshot(0).await).await.unwrap();
        repo.insert(test_epoch(1)).await.unwrap();
        repo.write_to_storage(test_snapshot(1).await).await.unwrap();

        #[cfg(feature = "std")]
        let lock = repo.storage.inner.lock().unwrap();
        #[cfg(not(feature = "std"))]
        let lock = repo.storage.inner.lock();

        assert_eq!(lock.get(TEST_GROUP).unwrap().epoch_data.len(), 1);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn used_key_package_is_deleted() {
        let key_package_repo = InMemoryKeyPackageStorage::default();

        let key_package = test_member(TEST_PROTOCOL_VERSION, TEST_CIPHER_SUITE, b"member")
            .await
            .0;

        let (id, data) = key_package.to_storage().unwrap();

        key_package_repo.insert(id, data);

        let mut repo = GroupStateRepository::new(
            TEST_GROUP.to_vec(),
            InMemoryGroupStateStorage::new(),
            key_package_repo,
            Some(key_package.reference.clone()),
        )
        .unwrap();

        repo.key_package_repo.get(&key_package.reference).unwrap();

        repo.write_to_storage(test_snapshot(4).await).await.unwrap();

        assert!(repo.key_package_repo.get(&key_package.reference).is_none());
    }
}
