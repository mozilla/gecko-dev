// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use crate::SqLiteDataStorageError;
use mls_rs_core::psk::{ExternalPskId, PreSharedKey, PreSharedKeyStorage};
use rusqlite::{params, Connection, OptionalExtension};
use std::{
    ops::Deref,
    sync::{Arc, Mutex},
};

#[derive(Debug, Clone)]
/// SQLite storage for MLS pre-shared keys.
pub struct SqLitePreSharedKeyStorage {
    connection: Arc<Mutex<Connection>>,
}

impl SqLitePreSharedKeyStorage {
    pub(crate) fn new(connection: Connection) -> SqLitePreSharedKeyStorage {
        SqLitePreSharedKeyStorage {
            connection: Arc::new(Mutex::new(connection)),
        }
    }

    /// Insert a pre-shared key into storage.
    pub fn insert(&self, psk_id: Vec<u8>, psk: PreSharedKey) -> Result<(), SqLiteDataStorageError> {
        let connection = self.connection.lock().unwrap();

        // Upsert into the database
        connection
            .execute(
                "INSERT INTO psk (psk_id, data) VALUES (?,?) ON CONFLICT(psk_id) DO UPDATE SET data=excluded.data",
                params![psk_id, psk.deref()],
            )
            .map(|_| ())
            .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))
    }

    /// Get a pre-shared key from storage based on a unique id.
    pub fn get(&self, psk_id: &[u8]) -> Result<Option<PreSharedKey>, SqLiteDataStorageError> {
        let connection = self.connection.lock().unwrap();

        connection
            .query_row(
                "SELECT data FROM psk WHERE psk_id = ?",
                params![psk_id],
                |row| Ok(PreSharedKey::new(row.get(0)?)),
            )
            .optional()
            .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))
    }

    /// Delete a pre-shared key from storage based on a unique id.
    pub fn delete(&self, psk_id: &[u8]) -> Result<(), SqLiteDataStorageError> {
        let connection = self.connection.lock().unwrap();

        connection
            .execute("DELETE FROM psk WHERE psk_id = ?", params![psk_id])
            .map(|_| ())
            .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(mls_build_async, maybe_async::must_be_async)]
impl PreSharedKeyStorage for SqLitePreSharedKeyStorage {
    type Error = SqLiteDataStorageError;

    async fn get(&self, id: &ExternalPskId) -> Result<Option<PreSharedKey>, Self::Error> {
        self.get(id)
            .map_err(|e| SqLiteDataStorageError::DataConversionError(e.into()))
    }
}

#[cfg(test)]
mod tests {
    use mls_rs_core::psk::PreSharedKey;

    use crate::{
        SqLiteDataStorageEngine,
        {connection_strategy::MemoryStrategy, test_utils::gen_rand_bytes},
    };

    use super::SqLitePreSharedKeyStorage;

    fn test_psk() -> (Vec<u8>, PreSharedKey) {
        let psk_id = gen_rand_bytes(32);
        let stored_psk = PreSharedKey::new(gen_rand_bytes(64));

        (psk_id, stored_psk)
    }

    fn test_storage() -> SqLitePreSharedKeyStorage {
        SqLiteDataStorageEngine::new(MemoryStrategy)
            .unwrap()
            .pre_shared_key_storage()
            .unwrap()
    }

    #[test]
    fn test_insert() {
        let (psk_id, psk) = test_psk();
        let storage = test_storage();

        storage.insert(psk_id.clone(), psk.clone()).unwrap();

        let from_storage = storage.get(&psk_id).unwrap().unwrap();
        assert_eq!(from_storage, psk);
    }

    #[test]
    fn test_insert_existing_overwrite() {
        let (psk_id, psk) = test_psk();
        let (_, new_psk) = test_psk();

        let storage = test_storage();

        storage.insert(psk_id.clone(), psk).unwrap();
        storage.insert(psk_id.clone(), new_psk.clone()).unwrap();

        let from_storage = storage.get(&psk_id).unwrap().unwrap();
        assert_eq!(from_storage, new_psk);
    }

    #[test]
    fn test_delete() {
        let (psk_id, psk) = test_psk();
        let storage = test_storage();

        storage.insert(psk_id.clone(), psk).unwrap();
        storage.delete(&psk_id).unwrap();

        assert!(storage.get(&psk_id).unwrap().is_none());
    }
}
