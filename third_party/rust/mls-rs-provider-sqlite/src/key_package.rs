// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use mls_rs_core::{
    key_package::{KeyPackageData, KeyPackageStorage},
    mls_rs_codec::{MlsDecode, MlsEncode},
    time::MlsTime,
};
use rusqlite::{params, Connection, OptionalExtension};
use std::sync::{Arc, Mutex};

use crate::SqLiteDataStorageError;

#[derive(Debug, Clone)]
/// SQLite storage for MLS Key Packages.
pub struct SqLiteKeyPackageStorage {
    connection: Arc<Mutex<Connection>>,
}

impl SqLiteKeyPackageStorage {
    pub(crate) fn new(connection: Connection) -> SqLiteKeyPackageStorage {
        SqLiteKeyPackageStorage {
            connection: Arc::new(Mutex::new(connection)),
        }
    }

    fn insert(
        &mut self,
        id: &[u8],
        key_package: KeyPackageData,
    ) -> Result<(), SqLiteDataStorageError> {
        let connection = self.connection.lock().unwrap();

        connection
            .execute(
                "INSERT INTO key_package (id, expiration, data) VALUES (?,?,?)",
                params![
                    id,
                    key_package.expiration,
                    key_package
                        .mls_encode_to_vec()
                        .map_err(|e| SqLiteDataStorageError::DataConversionError(e.into()))?
                ],
            )
            .map(|_| ())
            .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))
    }

    fn get(&self, id: &[u8]) -> Result<Option<KeyPackageData>, SqLiteDataStorageError> {
        let connection = self.connection.lock().unwrap();

        connection
            .query_row(
                "SELECT data FROM key_package WHERE id = ?",
                params![id],
                |row| {
                    Ok(
                        KeyPackageData::mls_decode(&mut row.get::<_, Vec<u8>>(0)?.as_slice())
                            .unwrap(),
                    )
                },
            )
            .optional()
            .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))
    }

    /// Delete a specific key package from storage based on it's id.
    pub fn delete(&self, id: &[u8]) -> Result<(), SqLiteDataStorageError> {
        let connection = self.connection.lock().unwrap();

        connection
            .execute("DELETE FROM key_package where id = ?", params![id])
            .map(|_| ())
            .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))
    }

    pub fn delete_expired(&self) -> Result<(), SqLiteDataStorageError> {
        self.delete_expired_by_time(MlsTime::now().seconds_since_epoch())
    }

    pub fn delete_expired_by_time(&self, time: u64) -> Result<(), SqLiteDataStorageError> {
        let connection = self.connection.lock().unwrap();

        connection
            .execute(
                "DELETE FROM key_package where expiration < ?",
                params![time],
            )
            .map(|_| ())
            .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))
    }
}

#[cfg_attr(not(mls_build_async), maybe_async::must_be_sync)]
#[cfg_attr(mls_build_async, maybe_async::must_be_async)]
impl KeyPackageStorage for SqLiteKeyPackageStorage {
    type Error = SqLiteDataStorageError;

    async fn insert(&mut self, id: Vec<u8>, pkg: KeyPackageData) -> Result<(), Self::Error> {
        self.insert(id.as_slice(), pkg)
    }

    async fn get(&self, id: &[u8]) -> Result<Option<KeyPackageData>, Self::Error> {
        self.get(id)
    }

    async fn delete(&mut self, id: &[u8]) -> Result<(), Self::Error> {
        (*self).delete(id)
    }
}

#[cfg(test)]
mod tests {
    use super::SqLiteKeyPackageStorage;
    use crate::{
        SqLiteDataStorageEngine, SqLiteDataStorageError,
        {connection_strategy::MemoryStrategy, test_utils::gen_rand_bytes},
    };
    use assert_matches::assert_matches;
    use mls_rs_core::{crypto::HpkeSecretKey, key_package::KeyPackageData};

    fn test_storage() -> SqLiteKeyPackageStorage {
        SqLiteDataStorageEngine::new(MemoryStrategy)
            .unwrap()
            .key_package_storage()
            .unwrap()
    }

    fn test_key_package() -> (Vec<u8>, KeyPackageData) {
        let key_id = gen_rand_bytes(32);
        let key_package = KeyPackageData::new(
            gen_rand_bytes(256),
            HpkeSecretKey::from(gen_rand_bytes(256)),
            HpkeSecretKey::from(gen_rand_bytes(256)),
            123,
        );

        (key_id, key_package)
    }

    #[test]
    fn key_package_insert() {
        let mut storage = test_storage();
        let (key_package_id, key_package) = test_key_package();

        storage
            .insert(&key_package_id, key_package.clone())
            .unwrap();

        let from_storage = storage.get(&key_package_id).unwrap().unwrap();
        assert_eq!(from_storage, key_package);
    }

    #[test]
    fn duplicate_insert_should_fail() {
        let mut storage = test_storage();
        let (key_package_id, key_package) = test_key_package();

        storage
            .insert(&key_package_id, key_package.clone())
            .unwrap();

        let dupe_res = storage.insert(&key_package_id, key_package);

        assert_matches!(dupe_res, Err(SqLiteDataStorageError::SqlEngineError(_)));
    }

    #[test]
    fn key_package_not_found() {
        let mut storage = test_storage();
        let (key_package_id, key_package) = test_key_package();

        storage.insert(&key_package_id, key_package).unwrap();

        let (another_package_id, _) = test_key_package();

        assert!(storage.get(&another_package_id).unwrap().is_none());
    }

    #[test]
    fn key_package_delete() {
        let mut storage = test_storage();
        let (key_package_id, key_package) = test_key_package();

        storage.insert(&key_package_id, key_package).unwrap();

        storage.delete(&key_package_id).unwrap();
        assert!(storage.get(&key_package_id).unwrap().is_none());
    }

    #[test]
    fn expired_key_package_gelete() {
        let mut storage = test_storage();

        let data = [1, 15, 30, 1698652376].map(|exp| {
            let mut kp = test_key_package();
            kp.1.expiration = exp;
            kp
        });

        for (id, data) in &data {
            storage.insert(id, data.clone()).unwrap();
        }

        storage.delete_expired_by_time(30).unwrap();

        assert!(storage.get(&data[0].0).unwrap().is_none());
        assert!(storage.get(&data[1].0).unwrap().is_none());
        storage.get(&data[2].0).unwrap().unwrap();
        storage.get(&data[3].0).unwrap().unwrap();

        storage.delete_expired().unwrap();

        assert!(storage.get(&data[2].0).unwrap().is_none());
        assert!(storage.get(&data[3].0).unwrap().is_none());
    }
}
