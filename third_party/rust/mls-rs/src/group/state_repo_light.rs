// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::client::MlsError;
use crate::key_package::KeyPackageRef;

use alloc::vec::Vec;
use mls_rs_codec::MlsEncode;
use mls_rs_core::{
    error::IntoAnyError,
    group::{GroupState, GroupStateStorage},
    key_package::KeyPackageStorage,
};

use super::snapshot::Snapshot;

#[derive(Debug, Clone)]
pub(crate) struct GroupStateRepository<S, K>
where
    S: GroupStateStorage,
    K: KeyPackageStorage,
{
    pending_key_package_removal: Option<KeyPackageRef>,
    storage: S,
    key_package_repo: K,
}

impl<S, K> GroupStateRepository<S, K>
where
    S: GroupStateStorage,
    K: KeyPackageStorage,
{
    pub fn new(
        storage: S,
        key_package_repo: K,
        // Set to `None` if restoring from snapshot; set to `Some` when joining a group.
        key_package_to_remove: Option<KeyPackageRef>,
    ) -> Result<GroupStateRepository<S, K>, MlsError> {
        Ok(GroupStateRepository {
            storage,
            pending_key_package_removal: key_package_to_remove,
            key_package_repo,
        })
    }

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    pub async fn write_to_storage(&mut self, group_snapshot: Snapshot) -> Result<(), MlsError> {
        let group_state = GroupState {
            data: group_snapshot.mls_encode_to_vec()?,
            id: group_snapshot.state.context.group_id,
        };

        self.storage
            .write(group_state, Vec::new(), Vec::new())
            .await
            .map_err(|e| MlsError::GroupStorageError(e.into_any_error()))?;

        if let Some(ref key_package_ref) = self.pending_key_package_removal {
            self.key_package_repo
                .delete(key_package_ref)
                .await
                .map_err(|e| MlsError::KeyPackageRepoError(e.into_any_error()))?;
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use crate::{
        client::test_utils::{TEST_CIPHER_SUITE, TEST_PROTOCOL_VERSION},
        group::{
            snapshot::{test_utils::get_test_snapshot, Snapshot},
            test_utils::{test_member, TEST_GROUP},
        },
        storage_provider::in_memory::{InMemoryGroupStateStorage, InMemoryKeyPackageStorage},
    };

    use alloc::vec;

    use super::GroupStateRepository;

    #[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
    async fn test_snapshot(epoch_id: u64) -> Snapshot {
        get_test_snapshot(TEST_CIPHER_SUITE, epoch_id).await
    }

    #[maybe_async::test(not(mls_build_async), async(mls_build_async, crate::futures_test))]
    async fn test_stored_groups_list() {
        let mut test_repo = GroupStateRepository::new(
            InMemoryGroupStateStorage::default(),
            InMemoryKeyPackageStorage::default(),
            None,
        )
        .unwrap();

        test_repo
            .write_to_storage(test_snapshot(0).await)
            .await
            .unwrap();

        assert_eq!(test_repo.storage.stored_groups(), vec![TEST_GROUP])
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
            InMemoryGroupStateStorage::default(),
            key_package_repo,
            Some(key_package.reference.clone()),
        )
        .unwrap();

        repo.key_package_repo.get(&key_package.reference).unwrap();

        repo.write_to_storage(test_snapshot(4).await).await.unwrap();

        assert!(repo.key_package_repo.get(&key_package.reference).is_none());
    }
}
