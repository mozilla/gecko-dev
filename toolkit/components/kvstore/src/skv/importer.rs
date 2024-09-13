/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::borrow::Cow;

use rkv::{
    backend::{BackendDatabase, BackendEnvironment, BackendRwCursorTransaction},
    Readable, Rkv, SingleStore, StoreOptions, Value, Writer,
};
use xpcom::interfaces::nsIKeyValueImporter;

use crate::skv::{
    key::{Key as SkvKey, KeyError as SkvKeyError},
    store::{Store as SkvStore, StoreError as SkvStoreError},
    value::Value as SkvValue,
};

/// Imports key-value pairs from an Rkv database into Skv.
pub struct RkvImporter<'a, W, S> {
    writer: W,
    sources: Vec<(DatabaseName, S)>,
    store: &'a SkvStore,
    conflict_policy: ConflictPolicy,
    cleanup_policy: CleanupPolicy,
}

impl<'env, 'a, T, D> RkvImporter<'a, Writer<T>, SingleStore<D>>
where
    T: BackendRwCursorTransaction<'env, Database = D>,
    D: BackendDatabase,
{
    /// Creates an importer for a single Rkv database.
    pub fn for_database<'rkv: 'env, E>(
        env: &'rkv Rkv<E>,
        store: &'a SkvStore,
        name: &str,
    ) -> Result<Self, ImporterError>
    where
        E: BackendEnvironment<'env, Database = D, RwTransaction = T>,
    {
        Self::new(env, store, std::iter::once(name.to_owned().into()))
    }

    /// Creates an importer for all databases in an Rkv environment.
    pub fn for_all_databases<'rkv: 'env, E>(
        env: &'rkv Rkv<E>,
        store: &'a SkvStore,
    ) -> Result<Self, ImporterError>
    where
        E: BackendEnvironment<'env, Database = D, RwTransaction = T>,
    {
        let names = env
            .get_dbs()?
            .into_iter()
            .map(|name| name.try_into())
            .collect::<Result<Vec<_>, ImporterError>>()?;
        Self::new(env, store, names)
    }

    fn new<'rkv: 'env, E>(
        env: &'rkv Rkv<E>,
        store: &'a SkvStore,
        names: impl IntoIterator<Item = DatabaseName>,
    ) -> Result<Self, ImporterError>
    where
        E: BackendEnvironment<'env, Database = D, RwTransaction = T>,
    {
        let sources = names
            .into_iter()
            .map(|name| {
                let source = env.open_single(Some(name.as_str()), StoreOptions::default())?;
                Ok((name, source))
            })
            .collect::<Result<_, ImporterError>>()?;
        Ok(Self {
            writer: env.write()?,
            sources,
            store,
            conflict_policy: ConflictPolicy::Error,
            cleanup_policy: CleanupPolicy::Keep,
        })
    }

    /// Sets the conflict policy for this importer.
    pub fn conflict_policy(&mut self, policy: ConflictPolicy) -> &mut Self {
        self.conflict_policy = policy;
        self
    }

    /// Sets the cleanup policy for this importer.
    pub fn cleanup_policy(&mut self, policy: CleanupPolicy) -> &mut Self {
        self.cleanup_policy = policy;
        self
    }

    /// Copies all key-value pairs from the specified Rkv databases into
    /// the corresponding Skv databases.
    ///
    /// [`RkvImporter::import`] and [`RkvImporter::cleanup`] are exposed as
    /// separate methods because of lifetime requirements on the
    /// Rkv environment.
    pub fn import(&'env self) -> Result<(), ImporterError> {
        let writer = self.store.writer()?;
        writer.write(|tx| {
            for (name, store) in &self.sources {
                let importer = RkvSingleStoreImporter {
                    name,
                    reader: &self.writer,
                    store,
                    tx,
                    conflict_policy: self.conflict_policy,
                };
                importer.import()?;
            }
            Ok(())
        })
    }

    /// Cleans up imported key-value pairs from Rkv.
    pub fn cleanup(mut self) {
        match self.cleanup_policy {
            CleanupPolicy::Keep => {
                // Nothing to delete from Rkv, so no changes to commit.
                self.writer.abort();
            }
            CleanupPolicy::Delete => {
                for (_, store) in self.sources {
                    let _ = store.clear(&mut self.writer);
                }
                let _ = self.writer.commit();
            }
        }
    }
}

/// Copies all key-value pairs from a single Rkv database into
/// the corresponding Skv database.
struct RkvSingleStoreImporter<'a, 'rkv, 'conn, R, D> {
    name: &'a DatabaseName,
    reader: &'rkv R,
    store: &'a SingleStore<D>,
    tx: &'a rusqlite::Transaction<'conn>,
    conflict_policy: ConflictPolicy,
}

impl<'env, 'a, 'rkv: 'env, 'conn, R, D> RkvSingleStoreImporter<'a, 'rkv, 'conn, R, D>
where
    R: Readable<'env, Database = D>,
    D: BackendDatabase,
{
    fn import(&self) -> Result<(), ImporterError> {
        self.ensure_database_exists()?;
        for result in self.store.iter_start(self.reader)? {
            let (key, value) = result?;
            self.import_pair(key, value)?;
        }
        Ok(())
    }

    fn ensure_database_exists(&self) -> Result<(), ImporterError> {
        let mut statement = self
            .tx
            .prepare_cached("INSERT OR IGNORE INTO dbs(name) VALUES(:name)")?;
        statement.execute(rusqlite::named_params! {
            ":name": self.name.as_str(),
        })?;
        Ok(())
    }

    fn import_pair(&self, key: &[u8], value: rkv::Value) -> Result<(), ImporterError> {
        let key = SkvKey::try_from(Cow::Borrowed(
            std::str::from_utf8(key).map_err(|err| ImporterError::RkvKey(err.into()))?,
        ))?;
        let value = <SkvValue as From<serde_json::Value>>::from(match value {
            Value::Bool(b) => b.into(),
            Value::I64(n) => n.into(),
            Value::F64(n) => n.into_inner().into(),
            Value::Str(s) => s.into(),
            _ => Err(ImporterError::UnsupportedRkvValueType)?,
        });

        let mut statement = self.tx.prepare_cached(&format!(
            "INSERT INTO data(
               db_id,
               key,
               value
             )
             VALUES(
               (SELECT id FROM dbs WHERE name = :name),
               :key,
               jsonb(:value)
             )
             ON CONFLICT {}",
            match self.conflict_policy {
                ConflictPolicy::Error => {
                    // Throw an error if the values are different;
                    // do nothing if they're equal.
                    "DO UPDATE SET
                       value = throw(printf('conflict: %Q', key))
                     WHERE value <> excluded.value"
                }
                ConflictPolicy::Ignore => "DO NOTHING",
                ConflictPolicy::Replace => {
                    "DO UPDATE SET
                       value = excluded.value"
                }
            },
        ))?;
        statement.execute(rusqlite::named_params! {
            ":name": self.name.as_str(),
            ":key": key,
            ":value": value,
        })?;

        Ok(())
    }
}

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
struct DatabaseName(String);

impl From<String> for DatabaseName {
    fn from(value: String) -> Self {
        Self(value)
    }
}

impl TryFrom<Option<String>> for DatabaseName {
    type Error = ImporterError;

    fn try_from(value: Option<String>) -> Result<Self, Self::Error> {
        Ok(value.ok_or(ImporterError::UnnamedDatabase)?.into())
    }
}

impl DatabaseName {
    fn as_str(&self) -> &str {
        &self.0
    }
}

/// The action to take if a key in a source database already exists
/// and has a different value in the corresponding destination database.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(u8)]
pub enum ConflictPolicy {
    Error = nsIKeyValueImporter::ERROR_ON_CONFLICT,
    Ignore = nsIKeyValueImporter::IGNORE_ON_CONFLICT,
    Replace = nsIKeyValueImporter::REPLACE_ON_CONFLICT,
}

impl TryFrom<u8> for ConflictPolicy {
    type Error = ImporterError;

    fn try_from(value: u8) -> Result<Self, <Self as TryFrom<u8>>::Error> {
        Ok(match value {
            // Rust doesn't allow `as` expressions directly in
            // match arms.
            _ if value == Self::Error as u8 => Self::Error,
            _ if value == Self::Ignore as u8 => Self::Ignore,
            _ if value == Self::Replace as u8 => Self::Replace,
            _ => Err(ImporterError::UnknownConflictPolicy(value))?,
        })
    }
}

/// The action to take after a successful import.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(u8)]
pub enum CleanupPolicy {
    Keep = nsIKeyValueImporter::KEEP_AFTER_IMPORT,
    Delete = nsIKeyValueImporter::DELETE_AFTER_IMPORT,
}

impl TryFrom<u8> for CleanupPolicy {
    type Error = ImporterError;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        Ok(match value {
            _ if value == Self::Keep as u8 => Self::Keep,
            _ if value == Self::Delete as u8 => Self::Delete,
            _ => Err(ImporterError::UnknownCleanupPolicy(value))?,
        })
    }
}

#[derive(Debug, thiserror::Error)]
pub enum ImporterError {
    #[error("unknown conflict policy: {0}")]
    UnknownConflictPolicy(u8),
    #[error("unknown cleanup policy: {0}")]
    UnknownCleanupPolicy(u8),
    #[error("rkv store: {0}")]
    RkvStore(#[from] rkv::StoreError),
    #[error("skv store: {0}")]
    SkvStore(#[from] SkvStoreError),
    #[error("sqlite: {0}")]
    Sqlite(#[from] rusqlite::Error),
    #[error("rkv key: {0}")]
    RkvKey(Box<dyn std::error::Error + Send + Sync + 'static>),
    #[error("skv key: {0}")]
    SkvKey(#[from] SkvKeyError),
    #[error("unsupported rkv value type")]
    UnsupportedRkvValueType,
    #[error("unnamed database")]
    UnnamedDatabase,
}
