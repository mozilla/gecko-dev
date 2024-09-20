/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// Maintenance tasks to run on an SQLite database connection.
#[derive(Debug)]
pub struct Maintenance<'a> {
    conn: &'a rusqlite::Connection,
}

impl<'a> Maintenance<'a> {
    pub fn new(conn: &'a rusqlite::Connection) -> Self {
        Self { conn }
    }

    /// Quickly checks the database for consistency, without verifying indexes
    /// or checking for `UNIQUE` constraint violations.
    pub fn quick_check(&self) -> Result<(), MaintenanceError> {
        // We're interested in an `ok` / not OK result, so ask
        // SQLite to stop the analysis as soon as it finds 1 error.
        let ok = self.conn.query_row("PRAGMA quick_check(1)", [], |row| {
            Ok(row.get::<_, String>(0)? == "ok")
        })?;
        match ok {
            true => Ok(()),
            false => Err(MaintenanceError::QuickCheck),
        }
    }

    /// Checks the database for consistency.
    pub fn integrity_check(&self) -> Result<(), MaintenanceError> {
        let ok = self
            .conn
            .query_row("PRAGMA integrity_check(1)", [], |row| {
                Ok(row.get::<_, String>(0)? == "ok")
            })?;
        match ok {
            true => Ok(()),
            false => Err(MaintenanceError::IntegrityCheck),
        }
    }

    /// Checks for foreign key constraint violations.
    pub fn foreign_key_check(&self) -> Result<(), MaintenanceError> {
        // A foreign key check returns no rows on success.
        let ok = self
            .conn
            .prepare("PRAGMA foreign_key_check")
            .and_then(|mut statement| Ok(statement.query([])?.next()?.is_none()))?;
        match ok {
            true => Ok(()),
            false => Err(MaintenanceError::ForeignKeyCheck),
        }
    }

    /// Deletes and recreates all database indexes.
    pub fn reindex(&self) -> Result<(), MaintenanceError> {
        self.conn.execute_batch("REINDEX")?;
        Ok(())
    }
}

#[derive(thiserror::Error, Debug)]
pub enum MaintenanceError {
    #[error("quick check")]
    QuickCheck,
    #[error("integrity check")]
    IntegrityCheck,
    #[error("foreign key check")]
    ForeignKeyCheck,
    #[error("sqlite: {0}")]
    Sqlite(#[from] rusqlite::Error),
}
