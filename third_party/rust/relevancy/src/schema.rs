/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use rusqlite::{Connection, Transaction};
use sql_support::open_database::{self, ConnectionInitializer};

/// The current database schema version.
///
/// For any changes to the schema [`SQL`], please make sure to:
///
///  1. Bump this version.
///  2. Add a migration from the old version to the new version in
///     [`RelevancyConnectionInitializer::upgrade_from`].
pub const VERSION: u32 = 14;

/// The current database schema.
pub const SQL: &str = "
    CREATE TABLE url_interest(
        url_hash BLOB NOT NULL,
        interest_code INTEGER NOT NULL,
        PRIMARY KEY (url_hash, interest_code)
    ) WITHOUT ROWID;

    -- Stores user interest vectors.  The `kind` field stores the raw code from the `InterestVectorKind` enum.
    CREATE TABLE user_interest(
        kind INTEGER NOT NULL,
        interest_code INTEGER NOT NULL,
        count INTEGER NOT NULL,
        PRIMARY KEY (kind, interest_code)
    ) WITHOUT ROWID;
";

/// Initializes an SQLite connection to the Relevancy database, performing
/// migrations as needed.
pub struct RelevancyConnectionInitializer;

impl ConnectionInitializer for RelevancyConnectionInitializer {
    const NAME: &'static str = "relevancy db";
    const END_VERSION: u32 = VERSION;

    fn prepare(&self, conn: &Connection, _db_empty: bool) -> open_database::Result<()> {
        let initial_pragmas = "
            -- Use in-memory storage for TEMP tables.
            PRAGMA temp_store = 2;
            PRAGMA journal_mode = WAL;
            PRAGMA foreign_keys = ON;
        ";
        conn.execute_batch(initial_pragmas)?;
        Ok(())
    }

    fn init(&self, db: &Transaction<'_>) -> open_database::Result<()> {
        Ok(db.execute_batch(SQL)?)
    }

    fn upgrade_from(&self, tx: &Transaction<'_>, version: u32) -> open_database::Result<()> {
        match version {
            // Upgrades 1-12 are missing because we started with version 13, because of a
            // copy-and-paste error.
            13 => {
                tx.execute(
                    "
    CREATE TABLE user_interest(
        kind INTEGER NOT NULL,
        interest_code INTEGER NOT NULL,
        count INTEGER NOT NULL,
        PRIMARY KEY (kind, interest_code)
    ) WITHOUT ROWID;
                ",
                    (),
                )?;
                Ok(())
            }
            _ => Err(open_database::Error::IncompatibleVersion(version)),
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use sql_support::open_database::test_utils::MigratedDatabaseFile;

    /// The first database schema we used
    pub const V1_SCHEMA: &str = "
    CREATE TABLE url_interest(
        url_hash BLOB NOT NULL,
        interest_code INTEGER NOT NULL,
        PRIMARY KEY (url_hash, interest_code)
    ) WITHOUT ROWID;

        PRAGMA user_version=13;
    ";

    /// Test running all schema upgrades
    ///
    /// If an upgrade fails, then this test will fail with a panic.
    #[test]
    fn test_all_upgrades() {
        let db_file = MigratedDatabaseFile::new(RelevancyConnectionInitializer, V1_SCHEMA);
        db_file.run_all_upgrades();
        db_file.assert_schema_matches_new_database();
    }
}
