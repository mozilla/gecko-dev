/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use rkv::{
    backend::{BackendDatabase, BackendEnvironment, BackendRwCursorTransaction},
    Readable, Rkv, SingleStore, StoreOptions, Value, Writer,
};
use xpcom::interfaces::nsIKeyValueImporter;

use crate::skv::{
    connection::{ConnectionIncident, ToConnectionIncident},
    key::Key as SkvKey,
    store::{Store as SkvStore, StoreError as SkvStoreError},
    value::Value as SkvValue,
};

/// Specifies the databases to import.
#[derive(Clone, Debug)]
pub enum SourceDatabases {
    /// Import all key-value pairs from one or more named databases.
    Named(Vec<NamedSourceDatabase>),

    /// Import all key-value pairs from all databases.
    All {
        conflict_policy: ConflictPolicy,
        cleanup_policy: CleanupPolicy,
    },
}

/// Specifies the name and settings for a single database.
#[derive(Clone, Debug)]
pub struct NamedSourceDatabase {
    pub name: DatabaseName,
    pub conflict_policy: ConflictPolicy,
    pub cleanup_policy: CleanupPolicy,
}

/// Imports Rkv databases into Skv.
pub struct RkvImporter<'a, W, S> {
    writer: W,
    sources: Vec<(NamedSourceDatabase, S)>,
    store: &'a SkvStore,
}

impl<'env, 'a, T, D> RkvImporter<'a, Writer<T>, SingleStore<D>>
where
    T: BackendRwCursorTransaction<'env, Database = D>,
    D: BackendDatabase,
{
    /// Creates an importer for one or more Rkv databases in the same
    /// Rkv environment.
    pub fn new<'rkv: 'env, E>(
        env: &'rkv Rkv<E>,
        store: &'a SkvStore,
        dbs: SourceDatabases,
    ) -> Result<Self, ImporterError>
    where
        E: BackendEnvironment<'env, Database = D, RwTransaction = T>,
    {
        let dbs = match dbs {
            SourceDatabases::Named(dbs) => dbs,
            SourceDatabases::All {
                conflict_policy,
                cleanup_policy,
            } => env
                .get_dbs()?
                .into_iter()
                .map(|name| {
                    Ok(NamedSourceDatabase {
                        name: name.try_into()?,
                        conflict_policy,
                        cleanup_policy,
                    })
                })
                .collect::<Result<_, ImporterError>>()?,
        };
        let sources = dbs
            .into_iter()
            .map(|db| {
                let store = env.open_single(Some(db.name.as_str()), StoreOptions::default())?;
                Ok((db, store))
            })
            .collect::<Result<_, ImporterError>>()?;
        Ok(Self {
            writer: env.write()?,
            sources,
            store,
        })
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
            for (db, store) in &self.sources {
                let importer = RkvSingleStoreImporter {
                    name: &db.name,
                    reader: &self.writer,
                    store,
                    tx,
                    conflict_policy: db.conflict_policy,
                };
                importer.import()?;
            }
            Ok(())
        })
    }

    /// Cleans up imported key-value pairs from Rkv.
    pub fn cleanup(mut self) {
        for (db, store) in self.sources {
            match db.cleanup_policy {
                CleanupPolicy::Keep => continue,
                CleanupPolicy::Delete => {
                    let _ = store.clear(&mut self.writer);
                }
            }
        }
        let _ = self.writer.commit();
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
        let key = SkvKey::from(
            std::str::from_utf8(key).map_err(|err| ImporterError::RkvKey(err.into()))?,
        );
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
pub struct DatabaseName(String);

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
    #[error("unsupported rkv value type")]
    UnsupportedRkvValueType,
    #[error("unnamed database")]
    UnnamedDatabase,
}

impl ToConnectionIncident for ImporterError {
    fn to_incident(&self) -> Option<ConnectionIncident> {
        match self {
            Self::SkvStore(err) => err.to_incident(),
            Self::Sqlite(err) => err.to_incident(),
            _ => None,
        }
    }
}
