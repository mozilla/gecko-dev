/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Lower-level, generic SQLite connection management.
//!
//! This module is inspired by, and borrows concepts from, the
//! Application Services `sql-support` crate.

use std::{borrow::Cow, fmt::Debug, num::NonZeroU32, path::Path, sync::Mutex};

use rusqlite::{InterruptHandle, OpenFlags, Transaction, TransactionBehavior};

/// A path to a physical SQLite database, and optional [`OpenFlags`] for
/// interpreting that path.
pub trait ConnectionPath {
    fn as_path(&self) -> Cow<'_, Path>;

    fn flags(&self) -> OpenFlags;
}

/// Sets up an SQLite database connection, and either
/// initializes an empty physical database with the latest schema, or
/// upgrades an existing physical database to the latest schema.
pub trait ConnectionOpener {
    /// The highest schema version that we support.
    const MAX_SCHEMA_VERSION: u32;

    type Error: From<rusqlite::Error>;

    /// Sets up an opened connection for use. This is a good place to
    /// set pragmas and configuration options, register functions, and
    /// load extensions.
    fn setup(_conn: &mut rusqlite::Connection) -> Result<(), Self::Error> {
        Ok(())
    }

    /// Initializes an empty physical database with the latest schema.
    fn create(tx: &mut Transaction<'_>) -> Result<(), Self::Error>;

    /// Upgrades an existing physical database to the schema with
    /// the given version.
    fn upgrade(tx: &mut Transaction<'_>, to_version: NonZeroU32) -> Result<(), Self::Error>;
}

/// A thread-safe wrapper around a connection to a physical SQLite database.
pub struct Connection {
    /// The inner connection.
    conn: Mutex<rusqlite::Connection>,

    /// An object that's used to interrupt an ongoing SQLite operation.
    ///
    /// When [`InterruptHandle::interrupt`] is called on this thread, and
    /// another thread is currently running a database operation,
    /// that operation will be aborted, and the other thread can release
    /// its lock on the inner connection.
    interrupt_handle: InterruptHandle,
}

impl Connection {
    /// Opens a connection to a physical database at the given path.
    pub fn new<O>(path: &impl ConnectionPath, type_: ConnectionType) -> Result<Self, O::Error>
    where
        O: ConnectionOpener,
    {
        let mut conn = rusqlite::Connection::open_with_flags(
            path.as_path(),
            path.flags().union(type_.flags()),
        )?;
        O::setup(&mut conn)?;
        match type_ {
            ConnectionType::ReadOnly => Ok(Self::with_connection(conn)),
            ConnectionType::ReadWrite => {
                // Read-write connections should upgrade the schema to
                // the latest version.
                let mut tx = conn.transaction_with_behavior(TransactionBehavior::Exclusive)?;
                match tx.query_row_and_then("PRAGMA user_version", [], |row| row.get(0)) {
                    Ok(mut version @ 1..) => {
                        while version < O::MAX_SCHEMA_VERSION {
                            O::upgrade(&mut tx, NonZeroU32::new(version + 1).unwrap())?;
                            version += 1;
                        }
                    }
                    Ok(0) => O::create(&mut tx)?,
                    Err(err) => Err(err)?,
                }
                // Set the schema version to the highest that we support.
                // If the current version is higher than ours, downgrade it,
                // so that upgrading to it again in the future can fix up any
                // invariants that our version might not uphold.
                tx.execute_batch(&format!("PRAGMA user_version = {}", O::MAX_SCHEMA_VERSION))?;
                tx.commit()?;
                Ok(Self::with_connection(conn))
            }
        }
    }

    fn with_connection(conn: rusqlite::Connection) -> Self {
        let interrupt_handle = conn.get_interrupt_handle();
        Self {
            conn: Mutex::new(conn),
            interrupt_handle,
        }
    }

    /// Accesses the database for reading.
    pub fn read<T, E>(
        &self,
        f: impl FnOnce(&rusqlite::Connection) -> Result<T, E>,
    ) -> Result<T, E> {
        let conn = self.conn.lock().unwrap();
        f(&*conn)
    }

    /// Accesses the database in a transaction for reading and writing.
    pub fn write<T, E>(&self, f: impl FnOnce(&mut Transaction<'_>) -> Result<T, E>) -> Result<T, E>
    where
        E: From<rusqlite::Error>,
    {
        let mut conn = self.conn.lock().unwrap();
        let mut tx = conn.transaction_with_behavior(TransactionBehavior::Immediate)?;
        let result = f(&mut tx)?;
        tx.commit()?;
        Ok(result)
    }

    /// Interrupts any ongoing operations.
    pub fn interrupt(&self) {
        self.interrupt_handle.interrupt()
    }

    /// Consumes this [`Connection`] and returns the inner
    /// [`rusqlite::Connection`].
    pub fn into_inner(self) -> rusqlite::Connection {
        Mutex::into_inner(self.conn).unwrap()
    }
}

impl Debug for Connection {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("Connection { .. }")
    }
}

/// The type of a physical SQLite database connection.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum ConnectionType {
    ReadOnly,
    ReadWrite,
}

impl ConnectionType {
    fn flags(self) -> OpenFlags {
        match self {
            ConnectionType::ReadOnly => {
                // We already guard the inner connection with a mutex, so
                // opt out of unnecessary locking with `SQLITE_OPEN_NO_MUTEX`.
                OpenFlags::SQLITE_OPEN_NO_MUTEX
                    | OpenFlags::SQLITE_OPEN_EXRESCODE
                    | OpenFlags::SQLITE_OPEN_READ_ONLY
            }
            ConnectionType::ReadWrite => {
                OpenFlags::SQLITE_OPEN_NO_MUTEX
                    | OpenFlags::SQLITE_OPEN_EXRESCODE
                    | OpenFlags::SQLITE_OPEN_CREATE
                    | OpenFlags::SQLITE_OPEN_READ_WRITE
            }
        }
    }
}
