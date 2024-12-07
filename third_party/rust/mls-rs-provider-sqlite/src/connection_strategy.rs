// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright by contributors to this project.
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

use std::path::{Path, PathBuf};

use rusqlite::Connection;

use crate::SqLiteDataStorageError;

#[cfg(any(feature = "sqlcipher", feature = "sqlcipher-bundled"))]
pub use crate::cipher::*;

/// Trait that helps to set up a SQLite database connection.
pub trait ConnectionStrategy {
    /// Connect to the SQLite database.
    fn make_connection(&self) -> Result<Connection, SqLiteDataStorageError>;
}

/// Connection strategy that creates an in-memory database.
pub struct MemoryStrategy;

impl ConnectionStrategy for MemoryStrategy {
    fn make_connection(&self) -> Result<Connection, SqLiteDataStorageError> {
        Connection::open_in_memory().map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))
    }
}

/// Connection strategy that connects to a database based on a file path.
pub struct FileConnectionStrategy {
    db_path: PathBuf,
}

impl FileConnectionStrategy {
    pub fn new(db_path: &Path) -> FileConnectionStrategy {
        FileConnectionStrategy {
            db_path: db_path.to_owned(),
        }
    }
}

impl ConnectionStrategy for FileConnectionStrategy {
    fn make_connection(&self) -> Result<Connection, SqLiteDataStorageError> {
        Connection::open(&self.db_path)
            .map_err(|e| SqLiteDataStorageError::SqlEngineError(e.into()))
    }
}
