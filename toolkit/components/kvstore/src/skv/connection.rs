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

/// An error that indicates a potential problem with an SQLite database.
pub trait ToConnectionIncident {
    fn to_incident(&self) -> Option<ConnectionIncident>;
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

/// A maintenance task to run on an SQLite database.
pub trait ConnectionMaintenanceTask {
    type Error;

    fn run(self, conn: &mut rusqlite::Connection) -> Result<(), Self::Error>;
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

    /// Anomalies that indicate potential problems with the database.
    incidents: Mutex<Vec<ConnectionIncident>>,
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
            incidents: Mutex::default(),
        }
    }

    /// Returns all incidents that have occurred on this connection.
    pub fn incidents(&self) -> ConnectionIncidents<'_> {
        ConnectionIncidents(self)
    }

    /// Accesses the database for reading.
    pub fn read<T, E>(&self, f: impl FnOnce(&rusqlite::Connection) -> Result<T, E>) -> Result<T, E>
    where
        E: ToConnectionIncident,
    {
        self.reporting(|conn| f(conn))
    }

    /// Accesses the database in a transaction for reading and writing.
    pub fn write<T, E>(&self, f: impl FnOnce(&mut Transaction<'_>) -> Result<T, E>) -> Result<T, E>
    where
        E: From<rusqlite::Error> + ToConnectionIncident,
    {
        self.reporting(|conn| {
            let mut tx = conn.transaction_with_behavior(TransactionBehavior::Immediate)?;
            let value = f(&mut tx)?;
            tx.commit()?;
            Ok(value)
        })
    }

    /// Runs a maintenance task on the database.
    pub fn maintenance<M>(&self, task: M) -> Result<(), M::Error>
    where
        M: ConnectionMaintenanceTask,
    {
        // Don't report new incidents, because maintenance errors likely
        // indicate that the database needs to be replaced.
        let mut conn = self.conn.lock().unwrap();
        task.run(&mut conn)
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

    /// Accesses the database and reports any incidents that occur.
    fn reporting<T, E>(
        &self,
        f: impl FnOnce(&mut rusqlite::Connection) -> Result<T, E>,
    ) -> Result<T, E>
    where
        E: ToConnectionIncident,
    {
        let result = {
            let mut conn = self.conn.lock().unwrap();
            f(&mut *conn)
        };
        result.inspect_err(|err| {
            if let Some(incident) = err.to_incident() {
                self.incidents.lock().unwrap().push(incident);
            }
        })
    }
}

impl Debug for Connection {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("Connection { .. }")
    }
}

/// Anomalies on an SQLite connection that indicate potential problems with
/// the database file.
#[derive(Debug)]
pub struct ConnectionIncidents<'a>(&'a Connection);

impl<'a> ConnectionIncidents<'a> {
    /// Transforms the connection's incidents into a different value.
    pub fn map<T>(self, f: impl FnOnce(UnresolvedIncidents<'_>) -> T) -> T {
        let mut incidents = self.0.incidents.lock().unwrap();
        f(UnresolvedIncidents(&mut incidents))
    }
}

#[derive(Debug)]
pub struct UnresolvedIncidents<'a>(&'a mut Vec<ConnectionIncident>);

impl<'a> UnresolvedIncidents<'a> {
    /// Returns an iterator over the connection's incidents.
    pub fn iter(&self) -> impl Iterator<Item = ConnectionIncident> + '_ {
        self.0.iter().copied()
    }

    /// Marks all incidents as resolved, removing them from the connection.
    pub fn resolve(self) {
        self.0.clear();
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

/// An anomaly on an SQLite connection that indicates a potential problem
/// with the database file.
///
/// Many SQLite errors are transient, but certain kinds of errors can be
/// caused by permanent database corruption. Each [`Connection`] tracks
/// occurrences of these errors as "incidents".
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum ConnectionIncident {
    CorruptFile,
    CorruptIndex,
    CorruptForeignKey,
}

impl ToConnectionIncident for rusqlite::Error {
    fn to_incident(&self) -> Option<ConnectionIncident> {
        let Self::SqliteFailure(err, message) = self else {
            return None;
        };
        Some(match (err.code, err.extended_code, message) {
            (rusqlite::ErrorCode::DatabaseCorrupt, rusqlite::ffi::SQLITE_CORRUPT_INDEX, _) => {
                ConnectionIncident::CorruptIndex
            }
            (rusqlite::ErrorCode::DatabaseCorrupt, _, _) => ConnectionIncident::CorruptFile,
            (rusqlite::ErrorCode::Unknown, _, Some(message))
                if message.contains("foreign key mismatch")
                    || message.contains("no such table") =>
            {
                // SQLite reports misconfigured foreign key constraints as
                // "DML errors" [1], which are surfaced as generic parse errors
                // at statement preparation time.
                //
                // [1]: https://www.sqlite.org/foreignkeys.html#fk_indexes
                ConnectionIncident::CorruptForeignKey
            }
            _ => return None,
        })
    }
}
