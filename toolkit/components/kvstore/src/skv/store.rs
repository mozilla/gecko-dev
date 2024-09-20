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
    fn open(&self) -> Result<OpenStoreGuard<'_>, StoreError> {
        let mut state = self.state.lock().unwrap();
        Ok(match &*state {
            StoreState::Created(path) => {
                let store = Arc::new(OpenStore::new(path)?);
                *state = StoreState::Open(store.clone());
                OpenStoreGuard::new(store, self.waiter.guard())
            }
            StoreState::Open(store) => OpenStoreGuard::new(store.clone(), self.waiter.guard()),
            StoreState::Closed => Err(StoreError::Closed)?,
        })
    }

    /// Returns the read-write connection to use for queries and updates.
    pub fn writer(&self) -> Result<Writer<'_>, StoreError> {
        Ok(Writer(self.open()?))
    }

    /// Returns the read-only connection to use for concurrent reads.
    pub fn reader(&self) -> Result<Reader<'_>, StoreError> {
        Ok(Reader(self.open()?))
    }

    /// Closes both connections to the physical database.
    pub fn close(&self) {
        // Take ownership of the connections, so that we can close them and
        // prevent any new reads or writes from starting.
        let store = match mem::replace(&mut *self.state.lock().unwrap(), StoreState::Closed) {
            StoreState::Created(_) | StoreState::Closed => return,
            StoreState::Open(store) => store,
        };

        // Interrupt concurrent reads; there's no risk of data loss for them.
        store.reader.interrupt();

        // Wait for all pending operations (reads and writes) to finish and
        // drop their strong references to the store.
        self.waiter.wait();

        // Invariant: Waiting for all operations to finish should have dropped
        // all other strong references.
        let store = Arc::into_inner(store).expect("invariant violation");

        store.close();
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

/// A strong reference to an open store.
struct OpenStoreGuard<'a> {
    store: Arc<OpenStore>,
    // Field order is important here: struct fields are dropped in
    // declaration order, and we want to ensure that the strong reference
    // to the open store is dropped before the pending operation guard.
    // Otherwise, the strong reference count will race with the operation count,
    // and might violate the invariant in `Store::close()`.
    _guard: OperationGuard<'a>,
}

impl<'a> OpenStoreGuard<'a> {
    fn new(store: Arc<OpenStore>, guard: OperationGuard<'a>) -> Self {
        Self {
            store,
            _guard: guard,
        }
    }
}

/// A read-write connection to an SQLite database.
pub struct Writer<'a>(OpenStoreGuard<'a>);

impl<'a> Deref for Writer<'a> {
    type Target = Connection;

    fn deref(&self) -> &Self::Target {
        &self.0.store.writer
    }
}

/// A read-only connection to an SQLite database.
pub struct Reader<'a>(OpenStoreGuard<'a>);

impl<'a> Deref for Reader<'a> {
    type Target = Connection;

    fn deref(&self) -> &Self::Target {
        &self.0.store.reader
    }
}

#[derive(Debug)]
enum StoreState {
    Created(StorePath),
    Open(Arc<OpenStore>),
    Closed,
}

#[derive(Debug)]
struct OpenStore {
    writer: Connection,
    reader: Connection,
}

impl OpenStore {
    fn new(path: &StorePath) -> Result<Self, StoreError> {
        // Order is important here: the writer must be opened first,
        // so that it can initialize the schema.
        let writer = Connection::new::<Schema>(path, ConnectionType::ReadWrite)?;
        let reader = Connection::new::<Schema>(path, ConnectionType::ReadOnly)?;
        Ok(Self { writer, reader })
    }

    fn close(self) {
        // We can't meaningfully recover from failing to close
        // either connection, so ignore errors.
        let _ = self.reader.into_inner().close();
        let _ = self.writer.into_inner().close();
    }
}

#[derive(Debug)]
struct OperationWaiter {
    count: Mutex<usize>,
    cvar: Condvar,
}

impl OperationWaiter {
    fn new() -> Self {
        Self {
            count: Mutex::new(0),
            cvar: Condvar::new(),
        }
    }

    /// Increments the pending operation count, and returns a guard
    /// that decrements the count when dropped.
    fn guard(&self) -> OperationGuard<'_> {
        *self.count.lock().unwrap() += 1;
        OperationGuard(self)
    }

    /// Waits for the pending operation count to reach zero.
    fn wait(&self) {
        let mut count = self.count.lock().unwrap();
        while *count > 0 {
            count = self.cvar.wait(count).unwrap();
        }
    }
}

struct OperationGuard<'a>(&'a OperationWaiter);

impl<'a> Drop for OperationGuard<'a> {
    fn drop(&mut self) {
        let mut count = self.0.count.lock().unwrap();
        *count -= 1;
        if *count == 0 {
            self.0.cvar.notify_all();
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
