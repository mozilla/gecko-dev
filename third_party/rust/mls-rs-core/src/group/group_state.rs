// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use core::fmt::{self, Debug};

use crate::error::IntoAnyError;
#[cfg(mls_build_async)]
use alloc::boxed::Box;
use alloc::vec::Vec;

/// Generic representation of a group's state.
#[derive(Clone, PartialEq, Eq)]
pub struct GroupState {
    /// A unique group identifier.
    pub id: Vec<u8>,
    pub data: Vec<u8>,
}

impl Debug for GroupState {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("GroupState")
            .field("id", &crate::debug::pretty_bytes(&self.id))
            .field("data", &crate::debug::pretty_bytes(&self.data))
            .finish()
    }
}

/// Generic representation of a prior epoch.
#[derive(Clone, PartialEq, Eq)]
pub struct EpochRecord {
    /// A unique epoch identifier within a particular group.
    pub id: u64,
    pub data: Vec<u8>,
}

impl Debug for EpochRecord {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("EpochRecord")
            .field("id", &self.id)
            .field("data", &crate::debug::pretty_bytes(&self.data))
            .finish()
    }
}

impl EpochRecord {
    pub fn new(id: u64, data: Vec<u8>) -> Self {
        Self { id, data }
    }
}

/// Storage that can persist and reload a group state.
///
/// A group state is recorded as a combination of the current state
/// (represented by the [`GroupState`] trait) and some number of prior
/// group states (represented by the [`EpochRecord`] trait).
/// This trait implements reading and writing group data as requested by the protocol
/// implementation.
///
/// # Cleaning up records
///
/// Group state will not be purged when the local member is removed from the
/// group. It is up to the implementer of this trait to provide a mechanism
/// to delete records that can be used by an application.
///

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(mls_build_async, maybe_async::must_be_async)]
pub trait GroupStateStorage: Send + Sync {
    type Error: IntoAnyError;

    /// Fetch a group state from storage.
    async fn state(&self, group_id: &[u8]) -> Result<Option<Vec<u8>>, Self::Error>;

    /// Lazy load cached epoch data from a particular group.
    async fn epoch(&self, group_id: &[u8], epoch_id: u64) -> Result<Option<Vec<u8>>, Self::Error>;

    /// Write pending state updates.
    ///
    /// The group id that this update belongs to can be retrieved with
    /// [`GroupState::id`]. Prior epoch id values can be retrieved with
    /// [`EpochRecord::id`].
    ///
    /// The protocol implementation handles managing the max size of a prior epoch
    /// cache and the deleting of prior states based on group activity.
    /// The maximum number of prior epochs that will be stored is controlled by the
    /// `Preferences::max_epoch_retention` function in `mls_rs`.
    /// value. Requested deletes are communicated by the `delete_epoch_under`
    /// parameter being set to `Some`.
    ///
    /// # Warning
    ///
    /// It is important to consider error recovery when creating an implementation
    /// of this trait. Calls to [`write`](GroupStateStorage::write) should
    /// optimally be a single atomic transaction in order to avoid partial writes
    /// that may corrupt the group state.
    async fn write(
        &mut self,
        state: GroupState,
        epoch_inserts: Vec<EpochRecord>,
        epoch_updates: Vec<EpochRecord>,
    ) -> Result<(), Self::Error>;

    /// The [`EpochRecord::id`] value that is associated with a stored
    /// prior epoch for a particular group.
    async fn max_epoch_id(&self, group_id: &[u8]) -> Result<Option<u64>, Self::Error>;
}
