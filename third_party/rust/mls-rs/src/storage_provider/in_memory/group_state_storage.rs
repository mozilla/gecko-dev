// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use alloc::collections::VecDeque;

#[cfg(target_has_atomic = "ptr")]
use alloc::sync::Arc;

#[cfg(mls_build_async)]
use alloc::boxed::Box;
use alloc::vec::Vec;
use core::{
    convert::Infallible,
    fmt::{self, Debug},
};
use mls_rs_core::group::{EpochRecord, GroupState, GroupStateStorage};
#[cfg(not(target_has_atomic = "ptr"))]
use portable_atomic_util::Arc;

use crate::client::MlsError;

#[cfg(feature = "std")]
use std::collections::{hash_map::Entry, HashMap};

#[cfg(not(feature = "std"))]
use alloc::collections::{btree_map::Entry, BTreeMap};

#[cfg(feature = "std")]
use std::sync::Mutex;

#[cfg(not(feature = "std"))]
use spin::Mutex;

pub(crate) const DEFAULT_EPOCH_RETENTION_LIMIT: usize = 3;

#[derive(Clone)]
pub(crate) struct InMemoryGroupData {
    pub(crate) state_data: Vec<u8>,
    pub(crate) epoch_data: VecDeque<EpochRecord>,
}

impl Debug for InMemoryGroupData {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("InMemoryGroupData")
            .field(
                "state_data",
                &mls_rs_core::debug::pretty_bytes(&self.state_data),
            )
            .field("epoch_data", &self.epoch_data)
            .finish()
    }
}

impl InMemoryGroupData {
    pub fn new(state_data: Vec<u8>) -> InMemoryGroupData {
        InMemoryGroupData {
            state_data,
            epoch_data: Default::default(),
        }
    }

    fn get_epoch_data_index(&self, epoch_id: u64) -> Option<u64> {
        self.epoch_data
            .front()
            .and_then(|e| epoch_id.checked_sub(e.id))
    }

    pub fn get_epoch(&self, epoch_id: u64) -> Option<&EpochRecord> {
        self.get_epoch_data_index(epoch_id)
            .and_then(|i| self.epoch_data.get(i as usize))
    }

    pub fn get_mut_epoch(&mut self, epoch_id: u64) -> Option<&mut EpochRecord> {
        self.get_epoch_data_index(epoch_id)
            .and_then(|i| self.epoch_data.get_mut(i as usize))
    }

    pub fn insert_epoch(&mut self, epoch: EpochRecord) {
        self.epoch_data.push_back(epoch)
    }

    // This function does not fail if an update can't be made. If the epoch
    // is not in the store, then it can no longer be accessed by future
    // get_epoch calls and is no longer relevant.
    pub fn update_epoch(&mut self, epoch: EpochRecord) {
        if let Some(existing_epoch) = self.get_mut_epoch(epoch.id) {
            *existing_epoch = epoch
        }
    }

    pub fn trim_epochs(&mut self, max_epoch_retention: usize) {
        while self.epoch_data.len() > max_epoch_retention {
            self.epoch_data.pop_front();
        }
    }
}

#[derive(Clone)]
/// In memory group state storage backed by a HashMap.
///
/// All clones of an instance of this type share the same underlying HashMap.
pub struct InMemoryGroupStateStorage {
    #[cfg(feature = "std")]
    pub(crate) inner: Arc<Mutex<HashMap<Vec<u8>, InMemoryGroupData>>>,
    #[cfg(not(feature = "std"))]
    pub(crate) inner: Arc<Mutex<BTreeMap<Vec<u8>, InMemoryGroupData>>>,
    pub(crate) max_epoch_retention: usize,
}

impl Debug for InMemoryGroupStateStorage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("InMemoryGroupStateStorage")
            .field(
                "inner",
                &mls_rs_core::debug::pretty_with(|f| {
                    f.debug_map()
                        .entries(
                            self.lock()
                                .iter()
                                .map(|(k, v)| (mls_rs_core::debug::pretty_bytes(k), v)),
                        )
                        .finish()
                }),
            )
            .field("max_epoch_retention", &self.max_epoch_retention)
            .finish()
    }
}

impl InMemoryGroupStateStorage {
    /// Create an empty group state storage.
    pub fn new() -> Self {
        Self {
            inner: Default::default(),
            max_epoch_retention: DEFAULT_EPOCH_RETENTION_LIMIT,
        }
    }

    pub fn with_max_epoch_retention(self, max_epoch_retention: usize) -> Result<Self, MlsError> {
        (max_epoch_retention > 0)
            .then_some(())
            .ok_or(MlsError::NonZeroRetentionRequired)?;

        Ok(Self {
            inner: self.inner,
            max_epoch_retention,
        })
    }

    /// Get the set of unique group ids that have data stored.
    pub fn stored_groups(&self) -> Vec<Vec<u8>> {
        self.lock().keys().cloned().collect()
    }

    /// Delete all data corresponding to `group_id`.
    pub fn delete_group(&self, group_id: &[u8]) {
        self.lock().remove(group_id);
    }

    #[cfg(feature = "std")]
    fn lock(&self) -> std::sync::MutexGuard<'_, HashMap<Vec<u8>, InMemoryGroupData>> {
        self.inner.lock().unwrap()
    }

    #[cfg(not(feature = "std"))]
    fn lock(&self) -> spin::mutex::MutexGuard<'_, BTreeMap<Vec<u8>, InMemoryGroupData>> {
        self.inner.lock()
    }
}

impl Default for InMemoryGroupStateStorage {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(mls_build_async, maybe_async::must_be_async)]
impl GroupStateStorage for InMemoryGroupStateStorage {
    type Error = Infallible;

    async fn max_epoch_id(&self, group_id: &[u8]) -> Result<Option<u64>, Self::Error> {
        Ok(self
            .lock()
            .get(group_id)
            .and_then(|group_data| group_data.epoch_data.back().map(|e| e.id)))
    }

    async fn state(&self, group_id: &[u8]) -> Result<Option<Vec<u8>>, Self::Error> {
        Ok(self
            .lock()
            .get(group_id)
            .map(|data| data.state_data.clone()))
    }

    async fn epoch(&self, group_id: &[u8], epoch_id: u64) -> Result<Option<Vec<u8>>, Self::Error> {
        Ok(self
            .lock()
            .get(group_id)
            .and_then(|data| data.get_epoch(epoch_id).map(|ep| ep.data.clone())))
    }

    async fn write(
        &mut self,
        state: GroupState,
        epoch_inserts: Vec<EpochRecord>,
        epoch_updates: Vec<EpochRecord>,
    ) -> Result<(), Self::Error> {
        let mut group_map = self.lock();

        let group_data = match group_map.entry(state.id) {
            Entry::Occupied(entry) => {
                let data = entry.into_mut();
                data.state_data = state.data;
                data
            }
            Entry::Vacant(entry) => entry.insert(InMemoryGroupData::new(state.data)),
        };

        epoch_inserts
            .into_iter()
            .for_each(|e| group_data.insert_epoch(e));

        epoch_updates
            .into_iter()
            .for_each(|e| group_data.update_epoch(e));

        group_data.trim_epochs(self.max_epoch_retention);

        Ok(())
    }
}

#[cfg(all(test, feature = "prior_epoch"))]
mod tests {
    use alloc::{format, vec, vec::Vec};
    use assert_matches::assert_matches;

    use super::{InMemoryGroupData, InMemoryGroupStateStorage};
    use crate::{client::MlsError, group::test_utils::TEST_GROUP};

    use mls_rs_core::group::{EpochRecord, GroupState, GroupStateStorage};

    impl InMemoryGroupStateStorage {
        fn test_data(&self) -> InMemoryGroupData {
            self.lock().get(TEST_GROUP).unwrap().clone()
        }
    }

    fn test_storage(retention_limit: usize) -> Result<InMemoryGroupStateStorage, MlsError> {
        InMemoryGroupStateStorage::new().with_max_epoch_retention(retention_limit)
    }

    fn test_epoch(epoch_id: u64) -> EpochRecord {
        EpochRecord::new(epoch_id, format!("epoch {epoch_id}").as_bytes().to_vec())
    }

    fn test_snapshot(epoch_id: u64) -> GroupState {
        GroupState {
            id: TEST_GROUP.into(),
            data: format!("snapshot {epoch_id}").as_bytes().to_vec(),
        }
    }

    #[test]
    fn test_zero_max_retention() {
        assert_matches!(test_storage(0), Err(MlsError::NonZeroRetentionRequired))
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn existing_storage_can_have_larger_epoch_count() {
        let mut storage = test_storage(2).unwrap();

        let epoch_inserts = vec![test_epoch(0), test_epoch(1)];

        storage
            .write(test_snapshot(0), epoch_inserts, Vec::new())
            .await
            .unwrap();

        assert_eq!(storage.test_data().epoch_data.len(), 2);

        storage.max_epoch_retention = 4;

        let epoch_inserts = vec![test_epoch(3), test_epoch(4)];

        storage
            .write(test_snapshot(1), epoch_inserts, Vec::new())
            .await
            .unwrap();

        assert_eq!(storage.test_data().epoch_data.len(), 4);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn existing_storage_can_have_smaller_epoch_count() {
        let mut storage = test_storage(4).unwrap();

        let epoch_inserts = vec![test_epoch(0), test_epoch(1), test_epoch(3), test_epoch(4)];

        storage
            .write(test_snapshot(1), epoch_inserts, Vec::new())
            .await
            .unwrap();

        assert_eq!(storage.test_data().epoch_data.len(), 4);

        storage.max_epoch_retention = 2;

        let epoch_inserts = vec![test_epoch(5)];

        storage
            .write(test_snapshot(1), epoch_inserts, Vec::new())
            .await
            .unwrap();

        assert_eq!(storage.test_data().epoch_data.len(), 2);
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn epoch_insert_over_limit() {
        test_epoch_insert_over_limit(false).await
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn epoch_insert_over_limit_with_update() {
        test_epoch_insert_over_limit(true).await
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_epoch_insert_over_limit(with_update: bool) {
        let mut storage = test_storage(1).unwrap();

        let mut epoch_inserts = vec![test_epoch(0), test_epoch(1)];
        let updates = with_update
            .then_some(vec![test_epoch(0)])
            .unwrap_or_default();
        let snapshot = test_snapshot(1);

        storage
            .write(snapshot.clone(), epoch_inserts.clone(), updates)
            .await
            .unwrap();

        let stored = storage.test_data();

        assert_eq!(stored.state_data, snapshot.data);
        assert_eq!(stored.epoch_data.len(), 1);

        let expected = epoch_inserts.pop().unwrap();
        assert_eq!(stored.epoch_data[0], expected);
    }
}
