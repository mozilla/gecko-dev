// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use connection_strategy::ConnectionStrategy;
use group_state::SqLiteGroupStateStorage;
use psk::SqLitePreSharedKeyStorage;
use rusqlite::Connection;
use storage::{SqLiteApplicationStorage, SqLiteKeyPackageStorage};
use thiserror::Error;

mod application;
mod group_state;
mod key_package;
mod psk;

#[cfg(any(feature = "sqlcipher", feature = "sqlcipher-bundled"))]
mod cipher;

#[cfg(test)]
pub(crate) mod test_utils;

/// Connection strategies.
pub mod connection_strategy;

/// SQLite storage components.
pub mod storage {
    pub use {
        crate::application::{Item, SqLiteApplicationStorage},
        crate::group_state::SqLiteGroupStateStorage,
        crate::key_package::SqLiteKeyPackageStorage,
        crate::psk::SqLitePreSharedKeyStorage,
    };
}

#[derive(Debug, Error)]
/// SQLite data storage error.
pub enum SqLiteDataStorageError {
    #[error(transparent)]
    /// SQLite error.
    SqlEngineError(Box<dyn std::error::Error + Send + Sync + 'static>),
    #[error(transparent)]
    /// Stored data is not compatible with the expected data type.
    DataConversionError(Box<dyn std::error::Error + Send + Sync + 'static>),
    #[cfg(any(feature = "sqlcipher", feature = "sqlcipher-bundled"))]
    #[error("invalid key, must use SqlCipherKey::RawKeyWithSalt with plaintext_header_size > 0")]
    /// Invalid SQLCipher key header.
    SqlCipherKeyInvalidWithHeader,
}

impl mls_rs_core::error::IntoAnyError for SqLiteDataStorageError {
    fn into_dyn_error(self) -> Result<Box<dyn std::error::Error + Send + Sync>, Self> {
        Ok(self.into())
    }
}

#[derive(Clone, Debug)]
/// SQLite data storage engine.
pub struct SqLiteDataStorageEngine<CS>
where
    CS: ConnectionStrategy,
{
    connection_strategy: CS,
    group_state_context: Option<Vec<u8>>,
}

impl<CS> SqLiteDataStorageEngine<CS>
where
    CS: ConnectionStrategy,
{
    pub fn new(
        connection_strategy: CS,
    ) -> Result<SqLiteDataStorageEngine<CS>, SqLiteDataStorageError> {
        Ok(SqLiteDataStorageEngine {
            connection_strategy,
            group_state_context: None,
        })
    }

    pub fn with_context(self, group_state_context: Vec<u8>) -> Self {
        Self {
            group_state_context: Some(group_state_context),
            ..self
        }
    }

    fn create_connection(&self) -> Result<Connection, SqLiteDataStorageError> {
        let connection = self.connection_strategy.make_connection()?;

        // Run SQL to establish the schema
        let current_schema = connection
            .pragma_query_value(None, "user_version", |rows| rows.get::<_, u32>(0))
            .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))?;

        if current_schema != 1 {
            create_tables_v1(&connection)?;
        }

        Ok(connection)
    }

    /// Returns a struct that implements the `GroupStateStorage` trait for use in MLS.
    pub fn group_state_storage(&self) -> Result<SqLiteGroupStateStorage, SqLiteDataStorageError> {
        Ok(SqLiteGroupStateStorage::new(
            self.create_connection()?,
            self.group_state_context.clone(),
        ))
    }

    /// Returns a struct that implements the `KeyPackageStorage` trait for use in MLS.
    pub fn key_package_storage(&self) -> Result<SqLiteKeyPackageStorage, SqLiteDataStorageError> {
        Ok(SqLiteKeyPackageStorage::new(self.create_connection()?))
    }

    /// Returns a struct that implements the `PreSharedKeyStorage` trait for use in MLS.
    pub fn pre_shared_key_storage(
        &self,
    ) -> Result<SqLitePreSharedKeyStorage, SqLiteDataStorageError> {
        Ok(SqLitePreSharedKeyStorage::new(self.create_connection()?))
    }

    /// Returns a key value store that can be used to store application specific data.
    pub fn application_data_storage(
        &self,
    ) -> Result<SqLiteApplicationStorage, SqLiteDataStorageError> {
        Ok(SqLiteApplicationStorage::new(self.create_connection()?))
    }
}

fn create_tables_v1(connection: &Connection) -> Result<(), SqLiteDataStorageError> {
    connection
        .execute_batch(
            "BEGIN;
            CREATE TABLE mls_group (
                group_id BLOB PRIMARY KEY,
                snapshot BLOB NOT NULL
            ) WITHOUT ROWID;
            CREATE TABLE epoch (
                group_id BLOB,
                epoch_id INTEGER,
                epoch_data BLOB NOT NULL,
                FOREIGN KEY (group_id) REFERENCES mls_group (group_id) ON DELETE CASCADE
                PRIMARY KEY (group_id, epoch_id)
            ) WITHOUT ROWID;
            CREATE TABLE key_package (
                id BLOB PRIMARY KEY,
                expiration INTEGER,
                data BLOB NOT NULL
            ) WITHOUT ROWID;
            CREATE INDEX key_package_exp ON key_package (expiration);
            CREATE TABLE psk (
                psk_id BLOB PRIMARY KEY,
                data BLOB NOT NULL
            ) WITHOUT ROWID;
            CREATE TABLE kvs (
                key TEXT PRIMARY KEY,
                value BLOB NOT NULL
            ) WITHOUT ROWID;
            PRAGMA user_version = 1;
            COMMIT;",
        )
        .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))
}

#[cfg(test)]
mod tests {
    use crate::{connection_strategy::MemoryStrategy, SqLiteDataStorageEngine};

    #[test]
    pub fn user_version_test() {
        let database = SqLiteDataStorageEngine::new(MemoryStrategy).unwrap();

        let _connection = database.create_connection().unwrap();

        // Create another connection to make sure the migration doesn't try to happen again.
        let connection = database.create_connection().unwrap();

        // Run SQL to establish the schema
        let current_schema = connection
            .pragma_query_value(None, "user_version", |rows| rows.get::<_, u32>(0))
            .unwrap();

        assert_eq!(current_schema, 1);
    }
}
