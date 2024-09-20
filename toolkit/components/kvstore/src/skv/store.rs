/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! A single store.

use std::{
    borrow::Cow,
    ffi::OsStr,
    fmt::Write,
    mem,
    ops::Deref,
    path::{Path, PathBuf},
    sync::{
        atomic::{self, AtomicUsize},
        Arc, Condvar, Mutex,
    },
    time::SystemTime,
};

use chrono::{DateTime, Utc};
use rusqlite::OpenFlags;

use crate::skv::{
    checker::{Checker, CheckerAction, IntoChecker},
    connection::{
        Connection, ConnectionIncident, ConnectionIncidents, ConnectionMaintenanceTask,
        ConnectionPath, ConnectionType, ToConnectionIncident,
    },
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
    path: StorePath,
    state: Mutex<StoreState>,
    waiter: OperationWaiter,
}

impl Store {
    pub fn new(path: StorePath) -> Self {
        Self {
            path,
            state: Mutex::new(StoreState::Created),
            waiter: OperationWaiter::new(),
        }
    }

    /// Gets or opens both connections to the physical database.
    fn open<C>(&self) -> Result<OpenStoreGuard<'_>, StoreError>
    where
        for<'a> ConnectionIncidents<'a>: IntoChecker<C>,
        C: ConnectionMaintenanceTask,
        C::Error: std::error::Error + Send + Sync + 'static,
    {
        let guard = {
            // Scope for the locked state.
            let mut state = self.state.lock().unwrap();
            loop {
                let result = match &*state {
                    StoreState::Created => {
                        let store = Arc::new(OpenStore::new(&self.path)?);
                        *state = StoreState::Open(store);
                        // Advance the state machine, so that the checker can
                        // check the database on first use.
                        continue;
                    }
                    StoreState::Open(store) => {
                        let store = store.clone();
                        match IntoChecker::<C>::into_checker(store.writer.incidents()) {
                            CheckerAction::Skip => {
                                let guard = OpenStoreGuard::new(store, self.waiter.guard());
                                Ok(CheckedStore::Healthy(guard))
                            }
                            CheckerAction::Check(checker) => {
                                // Take the store temporarily out of service.
                                // Clients won't be able to read from or
                                // write to the store during maintenance, but
                                // will be able to close it.
                                let writer =
                                    Writer(OpenStoreGuard::new(store.clone(), self.waiter.guard()));
                                *state = StoreState::Maintenance(store);
                                Err(UnhealthyStore::Check(writer, checker))
                            }
                            CheckerAction::Replace => {
                                // Take the store permanently out of service.
                                *state = StoreState::Corrupt;
                                Err(UnhealthyStore::Replace(store))
                            }
                        }
                    }
                    StoreState::Maintenance(_) => return Err(StoreError::Busy),
                    StoreState::Corrupt => return Err(StoreError::Corrupt),
                    StoreState::Closed => return Err(StoreError::Closed),
                };
                break result;
            }
        }
        .or_else(|store| {
            match store {
                UnhealthyStore::Replace(store) => {
                    Ok(CheckedStore::Corrupt(store, StoreError::Corrupt))
                }
                UnhealthyStore::Check(writer, checker) => {
                    // Check for database corruption.
                    let result = writer
                        .maintenance(checker)
                        .map_err(|err| StoreError::Maintenance(err.into()));
                    {
                        // Scope for the locked state.
                        let mut state = self.state.lock().unwrap();
                        let StoreState::Maintenance(store) = &*state else {
                            // The store was closed during maintenance.
                            return result.and_then(|_| {
                                // The checker ran to completion, but
                                // the store is closed now.
                                Err(StoreError::Closed)
                            });
                        };
                        let store = store.clone();
                        match result {
                            Ok(()) => {
                                // If the checker succeeded, put the store
                                // back into service.
                                let guard = OpenStoreGuard::new(store.clone(), self.waiter.guard());
                                *state = StoreState::Open(store);
                                Ok(CheckedStore::Healthy(guard))
                            }
                            Err(err) => {
                                // If the checker failed, take the store
                                // permanently out of service.
                                *state = StoreState::Corrupt;
                                Ok(CheckedStore::Corrupt(store, err))
                            }
                        }
                    }
                }
            }
        })?;

        match guard {
            CheckedStore::Healthy(guard) => Ok(guard),
            CheckedStore::Corrupt(store, err) => {
                // Interrupt all connection operations. Since we're about
                // to replace the database, interrupting writes here is OK.
                store.reader.interrupt();
                store.writer.interrupt();

                // Wait for all (now-interrupted) operations to finish and
                // drop their strong references to the store.
                self.waiter.wait();

                // Invariant: Changing the state to `Corrupt`, and waiting for
                // all operations to finish, should have dropped all other
                // strong references.
                let store = Arc::into_inner(store).expect("invariant violation");

                store.close();

                // A corrupt database file might be salvageable, so
                // move it aside.
                if let Some(path) = self.path.on_disk() {
                    rename_corrupt_database_file(&path);
                }

                Err(err)
            }
        }
    }

    /// Returns the read-write connection to use for queries and updates.
    pub fn writer(&self) -> Result<Writer<'_>, StoreError> {
        Ok(Writer(self.open::<Checker>()?))
    }

    /// Returns the read-only connection to use for concurrent reads.
    pub fn reader(&self) -> Result<Reader<'_>, StoreError> {
        Ok(Reader(self.open::<Checker>()?))
    }

    #[cfg(feature = "gtest")]
    pub fn check<C>(&self) -> Result<(), StoreError>
    where
        for<'a> ConnectionIncidents<'a>: IntoChecker<C>,
        C: ConnectionMaintenanceTask,
        C::Error: std::error::Error + Send + Sync + 'static,
    {
        // We discard the guard because we only want to advance the
        // state machine, not return a connection.
        let _ = self.open::<C>()?;
        Ok(())
    }

    /// Closes both connections to the physical database.
    pub fn close(&self) {
        // Take ownership of the connections, so that we can close them and
        // prevent any new reads or writes from starting.
        let store = match mem::replace(&mut *self.state.lock().unwrap(), StoreState::Closed) {
            StoreState::Created | StoreState::Closed | StoreState::Corrupt => return,
            StoreState::Open(store) => {
                // If the store is working normally, interrupt concurrent reads,
                // but let writes finish.
                store.reader.interrupt();
                store
            }
            StoreState::Maintenance(store) => {
                // If the store is unhealthy, interrupt reads and writes.
                // There's no risk of data loss, because the writer is
                // only running maintenance operations, and we don't want
                // closing to wait on those.
                store.reader.interrupt();
                store.writer.interrupt();
                store
            }
        };

        // Wait for all connection operations to finish and drop their
        // strong references to the store.
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

    /// If this path is to a physical database file on disk,
    /// returns a reference to the path.
    pub fn on_disk(&self) -> Option<OnDiskStorePath<'_>> {
        match self {
            Self::OnDisk(buf) => buf
                .file_name()
                .map(|name| OnDiskStorePath::new(buf.parent(), name.into())),
            Self::InMemory(_) => None,
        }
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

/// A path to an SQLite database file and its related files on disk.
#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct OnDiskStorePath<'a> {
    dir: Option<&'a Path>,
    name: Cow<'a, OsStr>,
}

impl<'a> OnDiskStorePath<'a> {
    fn new(dir: Option<&'a Path>, name: Cow<'a, OsStr>) -> Self {
        Self { dir, name }
    }

    /// Returns the path to the temporary WAL file.
    pub fn wal(&self) -> PathBuf {
        let mut name = self.name.clone().into_owned();
        write!(&mut name, "-wal").unwrap();
        self.dir.map(|dir| dir.join(&name)).unwrap_or(name.into())
    }

    /// Returns the path to the temporary shared-memory file.
    pub fn shm(&self) -> PathBuf {
        let mut name = self.name.clone().into_owned();
        write!(&mut name, "-shm").unwrap();
        self.dir.map(|dir| dir.join(&name)).unwrap_or(name.into())
    }

    /// Returns the path to use for backing up a corrupt database file
    /// and its related files.
    pub fn to_corrupt(&self) -> OnDiskStorePath<'a> {
        let now = DateTime::<Utc>::from(SystemTime::now());
        let mut name = self.name.clone().into_owned();
        write!(&mut name, ".corrupt-{}", now.format("%Y%m%d%H%M%S")).unwrap();
        Self::new(self.dir, name.into())
    }
}

impl<'a> ConnectionPath for OnDiskStorePath<'a> {
    fn as_path(&self) -> Cow<'_, Path> {
        match self.dir {
            Some(dir) => Cow::Owned(dir.join(&self.name)),
            None => Cow::Borrowed(Path::new(&self.name)),
        }
    }

    fn flags(&self) -> OpenFlags {
        OpenFlags::empty()
    }
}

/// Backs up a corrupt SQLite database file and its related files.
fn rename_corrupt_database_file(source: &OnDiskStorePath<'_>) {
    let destination = source.to_corrupt();

    let _ = std::fs::rename(source.as_path(), destination.as_path());
    let _ = std::fs::rename(source.wal(), destination.wal());
    let _ = std::fs::rename(source.shm(), destination.shm());
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

/// The internal state of a [`Store`].
///
/// ## State diagram
///
/// ```custom
/// +---------+
/// | Created +-----------------------------------+
/// +--+------+                                   |
///    |                                          |
/// +--v---+    +-------------+    +---------+    |
/// | Open +----> Maintenance +----> Corrupt |    |
/// +-+--^-+    +---v--v------+    +----+----+    |
///   |  |          |  |                |         |
///   |  +----------+  |                |         |
///   |                |                |         |
///   | +--------------+                |         |
///   | |                               |         |
///   | | +-----------------------------+         |
///   | | |                                       |
/// +-v-v-v--+                                    |
/// | Closed <------------------------------------+
/// +--------+
/// ```
#[derive(Debug)]
enum StoreState {
    Created,
    Open(Arc<OpenStore>),
    Maintenance(Arc<OpenStore>),
    Corrupt,
    Closed,
}

#[derive(Debug)]
struct OpenStore {
    writer: Connection,
    reader: Connection,
}

impl OpenStore {
    fn new(path: &StorePath) -> Result<Self, StoreError> {
        Ok(match Self::open(path) {
            Ok(store) => store,
            Err(StoreError::Sqlite(err)) => {
                let (Some(code), Some(path)) = (err.sqlite_error_code(), path.on_disk()) else {
                    return Err(err.into());
                };
                match code {
                    rusqlite::ErrorCode::NotADatabase | rusqlite::ErrorCode::DatabaseCorrupt => {
                        // If SQLite can't open the database file, it's unlikely
                        // that we can salvage it, but move it aside anyway.
                        rename_corrupt_database_file(&path);
                        Self::open(&path)?
                    }
                    _ => return Err(err.into()),
                }
            }
            Err(err) => return Err(err),
        })
    }

    fn open(path: &impl ConnectionPath) -> Result<Self, StoreError> {
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

/// A temporarily out-of-service store.
enum UnhealthyStore<'a, C> {
    Check(Writer<'a>, C),
    Replace(Arc<OpenStore>),
}

/// An out-of-service store that was checked for corruption.
enum CheckedStore<'a> {
    Healthy(OpenStoreGuard<'a>),
    Corrupt(Arc<OpenStore>, StoreError),
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
    #[error("busy")]
    Busy,
    #[error("maintenance: {0}")]
    Maintenance(#[source] Box<dyn std::error::Error + Send + Sync + 'static>),
    #[error("closed")]
    Closed,
    #[error("corrupt")]
    Corrupt,
    #[error("sqlite: {0}")]
    Sqlite(#[from] rusqlite::Error),
}

impl ToConnectionIncident for StoreError {
    fn to_incident(&self) -> Option<ConnectionIncident> {
        match self {
            Self::Sqlite(err) => err.to_incident(),
            _ => None,
        }
    }
}
