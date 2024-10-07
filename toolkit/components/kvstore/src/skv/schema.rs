/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! The SQLite database schema.

use std::num::NonZeroU32;

use rusqlite::Transaction;

use crate::skv::connection::ConnectionMigrator;

/// The schema for a physical SQLite database that contains many
/// named logical databases.
#[derive(Debug)]
pub struct Schema;

impl ConnectionMigrator for Schema {
    const MAX_SCHEMA_VERSION: u32 = 1;

    type Error = SchemaError;

    fn create(tx: &mut Transaction<'_>) -> Result<(), Self::Error> {
        tx.execute_batch(
            "CREATE TABLE dbs(
               id INTEGER PRIMARY KEY,
               -- We expect each physical database to have just a handful of
               -- logical databases, so a UNIQUE constraint is OK here.
               name TEXT UNIQUE NOT NULL
             );

             CREATE TABLE data(
               -- Foreign key references don't create new automatic indexes,
               -- but `db_id` is the leftmost column of the primary key,
               -- so the primary key index is usable for cascading deletes.
               -- `db_id` and `text` also omit NOT NULL, because
               -- WITHOUT ROWID tables enforce NOT NULL for primary keys.
               db_id INTEGER REFERENCES dbs(id) ON DELETE CASCADE,
               key TEXT,
               value BLOB NOT NULL, -- Encoded in JSONB format.
               PRIMARY KEY(db_id, key)
             ) WITHOUT ROWID;

             CREATE TABLE meta(
               key TEXT PRIMARY KEY,
               value NOT NULL
             ) WITHOUT ROWID;
            ",
        )?;
        Ok(())
    }

    fn upgrade(_: &mut Transaction<'_>, to_version: NonZeroU32) -> Result<(), Self::Error> {
        Err(SchemaError::UnsupportedSchemaVersion(to_version.get()))
    }
}

#[derive(thiserror::Error, Debug)]
pub enum SchemaError {
    #[error("unsupported schema version: {0}")]
    UnsupportedSchemaVersion(u32),
    #[error("sqlite: {0}")]
    Sqlite(#[from] rusqlite::Error),
}
