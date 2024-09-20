/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! A single database in a store.

use std::{borrow::Borrow, ops::RangeBounds};

use rusqlite::ToSql;

use crate::skv::{
    key::Key,
    sql::RangeFragment,
    store::{Store, StoreError},
    value::Value,
};

struct Put<'a>(&'a Key, &'a Value);
struct Delete<'a>(&'a Key);

/// A data access object for reading and writing
/// keys and values in a named logical database.
pub struct Database<'a> {
    store: &'a Store,
    name: &'a str,
}

impl<'a> Database<'a> {
    pub fn new(store: &'a Store, name: &'a str) -> Self {
        Self { store, name }
    }

    pub fn has(&self, key: &Key, options: &GetOptions) -> Result<bool, DatabaseError> {
        self.getting(key, options, |statement, params| {
            Ok(statement.exists(params)?)
        })
    }

    pub fn get(&self, key: &Key, options: &GetOptions) -> Result<Option<Value>, DatabaseError> {
        self.getting(key, options, |statement, params| {
            let mut rows = statement.query(params)?;
            match rows.next()? {
                Some(row) => Ok(Some(row.get::<_, Value>("value")?)),
                None => Ok(None),
            }
        })
    }

    pub fn put<K, V>(&self, pairs: &[(K, Option<V>)]) -> Result<(), DatabaseError>
    where
        K: Borrow<Key>,
        V: Borrow<Value>,
    {
        let (updates, deletions) = pairs.iter().fold(
            (Vec::new(), Vec::new()),
            |(mut updates, mut deletions), (key, value)| {
                match value {
                    Some(value) => updates.push(Put(key.borrow(), value.borrow())),
                    None => deletions.push(Delete(key.borrow())),
                }
                (updates, deletions)
            },
        );
        self.put_or_delete(&updates, &deletions)
    }

    pub fn delete(&self, key: &Key) -> Result<(), DatabaseError> {
        self.put_or_delete(&[], &[Delete(key)])
    }

    pub fn delete_range(&self, range: impl RangeBounds<Key>) -> Result<(), DatabaseError> {
        let writer = self.store.writer()?;
        writer.write(|tx| {
            let fragment = RangeFragment::new("key", &range);
            let mut statement = tx.prepare_cached(&format!(
                "DELETE FROM
                   data
                 WHERE
                   db_id = (SELECT id FROM dbs WHERE name = :name) AND
                   {fragment}"
            ))?;
            let params = match (fragment.start_param(), fragment.end_param()) {
                (Some(p), Some(q)) => vec![(":name", &self.name as &dyn ToSql), p, q],
                (Some(p), None) | (None, Some(p)) => vec![(":name", &self.name as &dyn ToSql), p],
                (None, None) => vec![(":name", &self.name as &dyn ToSql)],
            };
            statement.execute(params.as_slice())?;
            Ok(())
        })
    }

    pub fn clear(&self) -> Result<(), DatabaseError> {
        let writer = self.store.writer()?;
        writer.write(|tx| {
            let mut statement = tx.prepare_cached("DELETE FROM dbs WHERE name = :name")?;
            statement.execute(rusqlite::named_params! {
                ":name": self.name,
            })?;
            Ok(())
        })
    }

    pub fn enumerate(
        &self,
        range: impl RangeBounds<Key>,
        options: &GetOptions,
    ) -> Result<Vec<(Key, Value)>, DatabaseError> {
        let r = |conn: &rusqlite::Connection| {
            let fragment = RangeFragment::new("v.key", &range);
            let mut statement = conn.prepare_cached(&format!(
                "SELECT
                   v.key,
                   json(v.value) AS value
                 FROM
                   data v
                 JOIN
                   dbs d
                   ON d.id = v.db_id
                 WHERE
                   d.name = :name
                   AND {fragment}
                 ORDER BY
                   v.key ASC
                ",
            ))?;
            let params = match (fragment.start_param(), fragment.end_param()) {
                // A bounded range binds parameters for the database name
                // and both key bounds.
                (Some(p), Some(q)) => vec![(":name", &self.name as &dyn ToSql), p, q],
                // A half-bounded range binds parameters for the database name
                // and only key bound.
                (Some(p), None) | (None, Some(p)) => vec![(":name", &self.name as &dyn ToSql), p],
                // An unbounded range only binds the database name.
                (None, None) => vec![(":name", &self.name as &dyn ToSql)],
            };
            let values = statement
                .query(params.as_slice())?
                .mapped(|row| {
                    let key = row.get::<_, Key>("key")?;
                    let value = row.get::<_, Value>("value")?;
                    Ok((key, value))
                })
                .collect::<rusqlite::Result<Vec<_>>>()?;
            Ok(values)
        };
        match options.concurrent {
            true => self.store.reader()?.read(r),
            false => self.store.writer()?.read(r),
        }
    }

    pub fn is_empty(&self) -> Result<bool, DatabaseError> {
        // Using the writer here ensures that we'll wait for any in-progress
        // transactions to commit, instead of returning an about-to-be-stale
        // result.
        let writer = self.store.writer()?;
        Ok(writer.read(|conn| -> Result<_, DatabaseError> {
            let mut statement = conn.prepare_cached(
                "SELECT 1 FROM data WHERE db_id = (SELECT id FROM dbs WHERE name = :name)",
            )?;
            let exists = statement.exists(rusqlite::named_params! { ":name": self.name })?;
            Ok(!exists)
        })?)
    }

    pub fn count(&self) -> Result<i64, DatabaseError> {
        // `is_empty` explains why we use the writer here.
        let writer = self.store.writer()?;
        Ok(writer.read(|conn| {
            conn.query_row(
                "SELECT count(*) FROM data WHERE db_id = (SELECT id FROM dbs WHERE name = :name)",
                rusqlite::named_params! { ":name": self.name },
                |row| row.get(0),
            )
        })?)
    }

    pub fn size(&self) -> Result<i64, DatabaseError> {
        // `is_empty` explains why we use the writer here.
        let writer = self.store.writer()?;
        Ok(writer.read(|conn| {
            conn.query_row(
                // `octet_length(column)` uses the metadata to calculate the
                // byte length of TEXT and BLOB values in the `column`, so
                // it's efficient even for large values.
                "SELECT
                   ifnull(sum(octet_length(key) + octet_length(value)), 0)
                 FROM
                   data
                 WHERE
                   db_id = (SELECT id FROM dbs WHERE name = :name)",
                rusqlite::named_params! { ":name": self.name },
                |row| row.get(0),
            )
        })?)
    }

    /// Prepares a statement that can be used to query or check
    /// the existence of a key.
    fn getting<T>(
        &self,
        key: &Key,
        options: &GetOptions,
        f: impl FnOnce(&mut rusqlite::Statement<'_>, &[(&str, &dyn ToSql)]) -> Result<T, DatabaseError>,
    ) -> Result<T, DatabaseError> {
        let r = |conn: &rusqlite::Connection| {
            let mut statement = conn.prepare_cached(
                "SELECT
                   json(v.value) AS value
                 FROM
                   data v
                 JOIN
                   dbs d
                   ON d.id = v.db_id
                 WHERE
                   d.name = :name
                   AND v.key = :key
                ",
            )?;
            let params = rusqlite::named_params! {
                ":name": &self.name,
                ":key": key,
            };
            f(&mut statement, params)
        };
        match options.concurrent {
            true => self.store.reader()?.read(r),
            false => self.store.writer()?.read(r),
        }
    }

    fn put_or_delete(&self, puts: &[Put], deletes: &[Delete]) -> Result<(), DatabaseError> {
        let writer = self.store.writer()?;
        writer.write(|tx| {
            if !puts.is_empty() {
                let mut statement =
                    tx.prepare_cached("INSERT OR IGNORE INTO dbs(name) VALUES(:name)")?;
                statement.execute(rusqlite::named_params! {
                    ":name": &self.name,
                })?;
            }

            for Put(key, value) in puts {
                let mut statement = tx.prepare_cached(
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
                     ON CONFLICT DO UPDATE SET
                       value = excluded.value",
                )?;
                statement.execute(rusqlite::named_params! {
                    ":name": &self.name,
                    ":key": key,
                    ":value": value,
                })?;
            }

            for Delete(key) in deletes {
                let mut statement = tx.prepare_cached(
                    "DELETE FROM data
                     WHERE
                       db_id = (SELECT id FROM dbs WHERE name = :name)
                       AND key = :key
                    ",
                )?;
                statement.execute(rusqlite::named_params! {
                    ":name": &self.name,
                    ":key": key,
                })?;
            }

            Ok(())
        })
    }
}

/// Options for reading keys and values.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GetOptions {
    concurrent: bool,
}

impl GetOptions {
    pub fn new() -> Self {
        Self { concurrent: false }
    }

    /// Sets the option for concurrent reads.
    ///
    /// If `true`, the read-only connection will be used to read from
    /// the database. Otherwise, the read-write connection will be used.
    pub fn concurrent(&mut self, concurrent: bool) -> &mut Self {
        self.concurrent = concurrent;
        self
    }
}

impl Default for GetOptions {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(thiserror::Error, Debug)]
pub enum DatabaseError {
    #[error("store: {0}")]
    Store(#[from] StoreError),
    #[error("sqlite: {0}")]
    Sqlite(#[from] rusqlite::Error),
}
