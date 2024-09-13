/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! The SQLite database schema.

use std::num::NonZeroU32;

use rusqlite::{
    config::DbConfig, functions::FunctionFlags, types::ToSqlOutput, ToSql, Transaction,
};

use crate::skv::connection::ConnectionOpener;

/// The schema for a physical SQLite database that contains many
/// named logical databases.
#[derive(Debug)]
pub struct Schema;

impl ConnectionOpener for Schema {
    const MAX_SCHEMA_VERSION: u32 = 1;

    type Error = SchemaError;

    fn setup(conn: &mut rusqlite::Connection) -> Result<(), Self::Error> {
        conn.execute_batch(
            "PRAGMA journal_mode = WAL;
             PRAGMA journal_size_limit = 512000; -- 512 KB.
             PRAGMA foreign_keys = ON;
             PRAGMA temp_store = MEMORY;
             PRAGMA secure_delete = ON;
             PRAGMA auto_vacuum = INCREMENTAL;
            ",
        )?;

        // Set hardening flags.
        conn.set_db_config(DbConfig::SQLITE_DBCONFIG_DEFENSIVE, true)?;

        // Turn off misfeatures: double-quoted strings and untrusted schemas.
        conn.set_db_config(DbConfig::SQLITE_DBCONFIG_DQS_DML, false)?;
        conn.set_db_config(DbConfig::SQLITE_DBCONFIG_DQS_DDL, false)?;
        conn.set_db_config(DbConfig::SQLITE_DBCONFIG_TRUSTED_SCHEMA, true)?;

        conn.create_scalar_function(
            // `throw(message)` throws an error with the given message.
            "throw",
            1,
            FunctionFlags::SQLITE_UTF8
                | FunctionFlags::SQLITE_DETERMINISTIC
                | FunctionFlags::SQLITE_DIRECTONLY,
            |context| -> rusqlite::Result<Never> {
                Err(UserFunctionError::Throw(context.get(0)?).into())
            },
        )?;

        Ok(())
    }

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

/// The `Ok` type for the `throw(message)` SQL function, which
/// always returns an `Err`.
///
/// This type can be removed once the ["never" type][1] (`!`) is
/// stabilized.
///
/// [1]: https://github.com/rust-lang/rust/issues/35121
enum Never {}

impl ToSql for Never {
    fn to_sql(&self) -> rusqlite::Result<ToSqlOutput<'_>> {
        unreachable!()
    }
}

#[derive(thiserror::Error, Debug)]
enum UserFunctionError {
    #[error("throw: {0}")]
    Throw(String),
}

impl Into<rusqlite::Error> for UserFunctionError {
    fn into(self) -> rusqlite::Error {
        rusqlite::Error::UserFunctionError(self.into())
    }
}

#[derive(thiserror::Error, Debug)]
pub enum SchemaError {
    #[error("unsupported schema version: {0}")]
    UnsupportedSchemaVersion(u32),
    #[error("sqlite: {0}")]
    Sqlite(#[from] rusqlite::Error),
}
