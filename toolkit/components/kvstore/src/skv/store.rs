/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! A single store.

use std::{
    borrow::Cow,
    mem,
    ops::Deref,
    path::{Path, PathBuf},
    sync::{
        atomic::{self, AtomicUsize},
        Arc, Condvar, Mutex,
    },
};

use rusqlite::OpenFlags;

use crate::skv::{
    connection::{Connection, ConnectionPath, ConnectionType},
    schema::{Schema, SchemaError},
};

/// A persistent store backed by a physical SQLite database.
///
/// Under the hood, a store holds two connections to the same physical database:
///
/// * A **read-write** connection for queries and updates. This connection
///   runs operations serially, and those operations can't be interrupted.
/// * A **read-only** connection for concurrent reads. This connection can
///   read from the physical database even if the read-write connection is busy,
///   and those reads can be interrupted. Reads on this connection won't see any
///   uncommitted changes on the read-write connection.
#[derive(Debug)]
pub struct Store {
    state: Mutex<StoreState>,
    waiter: OperationWaiter,
}

impl Store {
    pub fn new(path: StorePath) -> Self {
        Self {
            state: Mutex::new(StoreState::Created(path)),
            waiter: OperationWaiter::new(),
        }
    }

    /// Gets or opens both connections to the physical database.
    fn open(&self) -> Result<OpenStore, StoreError> {
        let mut state = self.state.lock().unwrap();
        Ok(match &*state {
            StoreState::Created(path) => {
                let store = OpenStore::new(path)?;
                *state = StoreState::Open(store.clone());
                store
            }
            StoreState::Open(store) => store.clone(),
            StoreState::Closed => Err(StoreError::Closed)?,
        })
    }

    /// Returns the read-write connection to use for queries and updates.
    pub fn writer(&self) -> Result<ConnectionGuard<'_>, StoreError> {
        Ok(ConnectionGuard::new(self.open()?.writer, &self.waiter))
    }

    /// Returns the read-only connection to use for concurrent reads.
    pub fn reader(&self) -> Result<ConnectionGuard<'_>, StoreError> {
        Ok(ConnectionGuard::new(self.open()?.reader, &self.waiter))
    }

    /// Closes both connections to the physical database.
    pub fn close(&self) {
        // Take ownership of the connections, so that we can close them and
        // prevent any new reads or writes from starting.
        let store = match mem::replace(&mut *self.state.lock().unwrap(), StoreState::Closed) {
            StoreState::Created(_) | StoreState::Closed => return,
            StoreState::Open(store) => store,
        };

        store.reader.interrupt();

        // Wait for all pending reads and writes to finish and
        // release their strong references to the connections.
        self.waiter.wait();

        // Invariant: Once all reads and writes have finished,
        // we should have the last strong reference to each connection.
        let reader = Arc::into_inner(store.reader).expect("invariant violation");
        let writer = Arc::into_inner(store.writer).expect("invariant violation");

        // We can't meaningfully recover from failing to close
        // either connection, so ignore errors.
        let _ = reader.into_inner().close();
        let _ = writer.into_inner().close();
    }
}

/// Either a path to a physical SQLite database file on disk, or
/// a reference to a unique in-memory database.
#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum StorePath {
    OnDisk(PathBuf),
    InMemory(usize),
}

impl StorePath {
    pub const IN_MEMORY_DATABASE_NAME: &'static str = ":memory:";

    const DEFAULT_DATABASE_FILE_NAME: &'static str = "kvstore.sqlite";

    /// Returns the path to the physical database file in the given
    /// storage directory.
    pub fn for_storage_dir(dir: impl Into<PathBuf>) -> Self {
        let mut path = dir.into();
        path.push(Self::DEFAULT_DATABASE_FILE_NAME);
        Self::OnDisk(path)
    }

    /// Returns a path to a unique in-memory physical database.
    pub fn for_in_memory() -> Self {
        static NEXT_IN_MEMORY_DATABASE_ID: AtomicUsize = AtomicUsize::new(1);
        let id = NEXT_IN_MEMORY_DATABASE_ID.fetch_add(1, atomic::Ordering::Relaxed);
        Self::InMemory(id)
    }
}

impl ConnectionPath for StorePath {
    fn as_path(&self) -> Cow<'_, Path> {
        match self {
            Self::OnDisk(buf) => Cow::Borrowed(buf.as_path()),
            Self::InMemory(id) => {
                // A store opens two connections to the same physical database.
                // To make this work for in-memory databases, we use a URI that
                // names the database and enables shared-cache mode.
                Cow::Owned(format!("file:kvstore-{id}?mode=memory&cache=shared").into())
            }
        }
    }

    fn flags(&self) -> OpenFlags {
        match self {
            Self::OnDisk(_) => OpenFlags::empty(),
            Self::InMemory(_) => {
                // Note that we must use a URI filename to open an
                // in-memory database in shared-cache mode.
                // SQLite will return a "library used incorrectly" error if
                // we try to open a named in-memory database with
                // `SQLITE_OPEN_MEMORY | SQLITE_OPEN_SHARED_CACHE`.
                OpenFlags::SQLITE_OPEN_URI
            }
        }
    }
}

pub struct ConnectionGuard<'a> {
    conn: Arc<Connection>,
    waiter: &'a OperationWaiter,
}

impl<'a> ConnectionGuard<'a> {
    fn new(conn: Arc<Connection>, waiter: &'a OperationWaiter) -> Self {
        *waiter.count.lock().unwrap() += 1;
        Self { conn, waiter }
    }
}

impl<'a> Deref for ConnectionGuard<'a> {
    type Target = Connection;

    fn deref(&self) -> &Self::Target {
        &self.conn
    }
}

impl<'a> Drop for ConnectionGuard<'a> {
    fn drop(&mut self) {
        let mut count = self.waiter.count.lock().unwrap();
        *count -= 1;
        self.waiter.monitor.notify_one();
    }
}

#[derive(Debug)]
enum StoreState {
    Created(StorePath),
    Open(OpenStore),
    Closed,
}

#[derive(Clone, Debug)]
struct OpenStore {
    writer: Arc<Connection>,
    reader: Arc<Connection>,
}

impl OpenStore {
    fn new(path: &StorePath) -> Result<Self, StoreError> {
        // Order is important here: the writer must be opened first,
        // so that it can initialize the schema.
        let writer = Connection::new::<Schema, _>(path, ConnectionType::ReadWrite)?;
        let reader = Connection::new::<Schema, _>(path, ConnectionType::ReadOnly)?;
        Ok(Self {
            writer: Arc::new(writer),
            reader: Arc::new(reader),
        })
    }
}

#[derive(Debug)]
struct OperationWaiter {
    count: Mutex<usize>,
    monitor: Condvar,
}

impl OperationWaiter {
    fn new() -> Self {
        Self {
            count: Mutex::new(0),
            monitor: Condvar::new(),
        }
    }

    /// Waits for the pending operation count to reach zero.
    fn wait(&self) {
        let mut count = self.count.lock().unwrap();
        while *count > 0 {
            count = self.monitor.wait(count).unwrap();
        }
    }
}

#[derive(thiserror::Error, Debug)]
pub enum StoreError {
    #[error("schema: {0}")]
    Schema(#[from] SchemaError),
    #[error("closed")]
    Closed,
    #[error("sqlite: {0}")]
    Sqlite(#[from] rusqlite::Error),
}
