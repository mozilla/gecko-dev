/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use crate::db::Sqlite3Extension;
use rusqlite::{Connection, Transaction};
use sql_support::{
    open_database::{self, ConnectionInitializer},
    ConnExt,
};

/// The current database schema version.
///
/// For any changes to the schema [`SQL`], please make sure to:
///
///  1. Bump this version.
///  2. Add a migration from the old version to the new version in
///     [`SuggestConnectionInitializer::upgrade_from`].
///    a. If suggestions should be re-ingested after the migration, call `clear_database()` inside
///       the migration.
pub const VERSION: u32 = 26;

/// The current Suggest database schema.
pub const SQL: &str = "
CREATE TABLE meta(
    key TEXT PRIMARY KEY,
    value NOT NULL
) WITHOUT ROWID;

CREATE TABLE rs_cache(
    collection TEXT PRIMARY KEY,
    data TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE ingested_records(
    id TEXT,
    collection TEXT,
    type TEXT NOT NULL,
    last_modified INTEGER NOT NULL,
    PRIMARY KEY (id, collection)
) WITHOUT ROWID;

CREATE TABLE keywords(
    keyword TEXT NOT NULL,
    suggestion_id INTEGER NOT NULL,
    full_keyword_id INTEGER NULL,
    rank INTEGER NOT NULL,
    PRIMARY KEY (keyword, suggestion_id)
) WITHOUT ROWID;

-- full keywords are what we display to the user when a (partial) keyword matches
CREATE TABLE full_keywords(
    id INTEGER PRIMARY KEY,
    suggestion_id INTEGER NOT NULL,
    full_keyword TEXT NOT NULL
);

CREATE TABLE prefix_keywords(
    keyword_prefix TEXT NOT NULL,
    keyword_suffix TEXT NOT NULL DEFAULT '',
    confidence INTEGER NOT NULL DEFAULT 0,
    rank INTEGER NOT NULL,
    suggestion_id INTEGER NOT NULL,
    PRIMARY KEY (keyword_prefix, keyword_suffix, suggestion_id)
) WITHOUT ROWID;

CREATE UNIQUE INDEX keywords_suggestion_id_rank ON keywords(suggestion_id, rank);

CREATE TABLE suggestions(
    id INTEGER PRIMARY KEY,
    record_id TEXT NOT NULL,
    provider INTEGER NOT NULL,
    title TEXT NOT NULL,
    url TEXT NOT NULL,
    score REAL NOT NULL
);

CREATE TABLE amp_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    advertiser TEXT NOT NULL,
    block_id INTEGER NOT NULL,
    iab_category TEXT NOT NULL,
    impression_url TEXT NOT NULL,
    click_url TEXT NOT NULL,
    icon_id TEXT NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);

CREATE TABLE wikipedia_custom_details(
    suggestion_id INTEGER PRIMARY KEY REFERENCES suggestions(id) ON DELETE CASCADE,
    icon_id TEXT NOT NULL
);

CREATE TABLE amo_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    description TEXT NOT NULL,
    guid TEXT NOT NULL,
    icon_url TEXT NOT NULL,
    rating TEXT,
    number_of_ratings INTEGER NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);

CREATE TABLE fakespot_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    fakespot_grade TEXT NOT NULL,
    product_id TEXT NOT NULL,
    keywords TEXT NOT NULL,
    product_type TEXT NOT NULL,
    rating REAL NOT NULL,
    total_reviews INTEGER NOT NULL,
    icon_id TEXT,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);

CREATE VIRTUAL TABLE IF NOT EXISTS fakespot_fts USING FTS5(
  title,
  content='',
  contentless_delete=1,
  tokenize=\"porter unicode61 remove_diacritics 2 tokenchars '''-'\"
);

CREATE TRIGGER fakespot_ai AFTER INSERT ON fakespot_custom_details BEGIN
  INSERT INTO fakespot_fts(rowid, title)
    SELECT id, title
    FROM suggestions
    WHERE id = new.suggestion_id;
END;

-- DELETE/UPDATE triggers are difficult to implement, since the FTS contents are split between the fakespot_custom_details and suggestions tables.
-- If you use an AFTER trigger, then the data from the other table has already been deleted.
-- BEFORE triggers are discouraged by the SQLite docs.
-- Instead, the drop_suggestions function handles updating the FTS data.

CREATE INDEX suggestions_record_id ON suggestions(record_id);

CREATE TABLE icons(
    id TEXT PRIMARY KEY,
    data BLOB NOT NULL,
    mimetype TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE yelp_subjects(
    keyword TEXT PRIMARY KEY,
    record_id TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE yelp_modifiers(
    type INTEGER NOT NULL,
    keyword TEXT NOT NULL,
    record_id TEXT NOT NULL,
    PRIMARY KEY (type, keyword)
) WITHOUT ROWID;

CREATE TABLE yelp_location_signs(
    keyword TEXT PRIMARY KEY,
    need_location INTEGER NOT NULL,
    record_id TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE yelp_custom_details(
    icon_id TEXT PRIMARY KEY,
    score REAL NOT NULL,
    record_id TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE mdn_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    description TEXT NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);

CREATE TABLE exposure_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    type TEXT NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);
CREATE INDEX exposure_custom_details_type ON exposure_custom_details(type);

CREATE TABLE dismissed_suggestions (
    url TEXT PRIMARY KEY
) WITHOUT ROWID;
";

/// Initializes an SQLite connection to the Suggest database, performing
/// migrations as needed.
#[derive(Default)]
pub struct SuggestConnectionInitializer<'a> {
    extensions_to_load: &'a [Sqlite3Extension],
}

impl<'a> SuggestConnectionInitializer<'a> {
    pub fn new(extensions_to_load: &'a [Sqlite3Extension]) -> Self {
        Self { extensions_to_load }
    }

    pub fn load_extensions(&self, conn: &Connection) -> open_database::Result<()> {
        // Safety: this relies on the extensions we're loading to operate correctly, for the
        // entry point to be correct, etc.
        unsafe {
            let _guard = rusqlite::LoadExtensionGuard::new(conn)?;
            for ext in self.extensions_to_load {
                conn.load_extension(&ext.library, ext.entry_point.as_deref())?;
            }
        }
        Ok(())
    }
}

impl ConnectionInitializer for SuggestConnectionInitializer<'_> {
    const NAME: &'static str = "suggest db";
    const END_VERSION: u32 = VERSION;

    fn prepare(&self, conn: &Connection, _db_empty: bool) -> open_database::Result<()> {
        self.load_extensions(conn)?;
        let initial_pragmas = "
            -- Use in-memory storage for TEMP tables.
            PRAGMA temp_store = 2;

            PRAGMA journal_mode = WAL;
            PRAGMA foreign_keys = ON;
        ";
        conn.execute_batch(initial_pragmas)?;
        sql_support::debug_tools::define_debug_functions(conn)?;

        Ok(())
    }

    fn init(&self, db: &Transaction<'_>) -> open_database::Result<()> {
        db.execute_batch(SQL)?;
        Ok(())
    }

    fn upgrade_from(&self, tx: &Transaction<'_>, version: u32) -> open_database::Result<()> {
        match version {
            1..=15 => {
                // Treat databases with these older schema versions as corrupt,
                // so that they'll be replaced by a fresh, empty database with
                // the current schema.
                Err(open_database::Error::Corrupt)
            }
            16 => {
                tx.execute(
                    "
CREATE TABLE dismissed_suggestions (
    url_hash INTEGER PRIMARY KEY
) WITHOUT ROWID;",
                    (),
                )?;
                Ok(())
            }
            17 => {
                tx.execute(
                    "
DROP TABLE dismissed_suggestions;
CREATE TABLE dismissed_suggestions (
    url TEXT PRIMARY KEY
) WITHOUT ROWID;",
                    (),
                )?;
                Ok(())
            }
            18 => {
                tx.execute_batch(
                    "
CREATE TABLE IF NOT EXISTS dismissed_suggestions (
    url TEXT PRIMARY KEY
) WITHOUT ROWID;",
                )?;
                Ok(())
            }
            19 => {
                // Clear the database since we're going to be dropping the keywords table and
                // re-creating it
                clear_database(tx)?;
                tx.execute_batch(
                    "
-- Recreate the various keywords table to drop the foreign keys.
DROP TABLE keywords;
DROP TABLE full_keywords;
DROP TABLE prefix_keywords;
CREATE TABLE keywords(
    keyword TEXT NOT NULL,
    suggestion_id INTEGER NOT NULL,
    full_keyword_id INTEGER NULL,
    rank INTEGER NOT NULL,
    PRIMARY KEY (keyword, suggestion_id)
) WITHOUT ROWID;
CREATE TABLE full_keywords(
    id INTEGER PRIMARY KEY,
    suggestion_id INTEGER NOT NULL,
    full_keyword TEXT NOT NULL
);
CREATE TABLE prefix_keywords(
    keyword_prefix TEXT NOT NULL,
    keyword_suffix TEXT NOT NULL DEFAULT '',
    confidence INTEGER NOT NULL DEFAULT 0,
    rank INTEGER NOT NULL,
    suggestion_id INTEGER NOT NULL,
    PRIMARY KEY (keyword_prefix, keyword_suffix, suggestion_id)
) WITHOUT ROWID;
CREATE UNIQUE INDEX keywords_suggestion_id_rank ON keywords(suggestion_id, rank);
                    ",
                )?;
                Ok(())
            }

            // Migration for the fakespot data.  This is not currently active for any users, it's
            // only used for the tests.  It's safe to alter the fakespot_custom_detail schema and
            // update this migration as the project moves forward.
            //
            // Note: if we want to add a regular migration while the fakespot code is still behind
            // a feature flag, insert it before this one and make fakespot the last migration.
            20 => {
                tx.execute_batch(
                    "
CREATE TABLE fakespot_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    fakespot_grade TEXT NOT NULL,
    product_id TEXT NOT NULL,
    rating REAL NOT NULL,
    total_reviews INTEGER NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);
-- Create the Fakespot FTS table.
-- The `tokenize` param is hard to read.  The effect is that dashes and apostrophes are
-- considered valid tokens in a word, rather than separators.
CREATE VIRTUAL TABLE IF NOT EXISTS fakespot_fts USING FTS5(
  title,
  prefix='4 5 6 7 8 9 10 11',
  content='',
  contentless_delete=1,
  tokenize=\"porter unicode61 remove_diacritics 2 tokenchars '''-'\"
);
CREATE TRIGGER fakespot_ai AFTER INSERT ON fakespot_custom_details BEGIN
  INSERT INTO fakespot_fts(rowid, title)
    SELECT id, title
    FROM suggestions
    WHERE id = new.suggestion_id;
END;
                ",
                )?;
                Ok(())
            }
            21 => {
                // Drop and re-create the fakespot_custom_details to add the icon_id column.
                tx.execute_batch(
                    "
DROP TABLE fakespot_custom_details;
CREATE TABLE fakespot_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    fakespot_grade TEXT NOT NULL,
    product_id TEXT NOT NULL,
    rating REAL NOT NULL,
    total_reviews INTEGER NOT NULL,
    icon_id TEXT,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);
CREATE TRIGGER fakespot_ai AFTER INSERT ON fakespot_custom_details BEGIN
  INSERT INTO fakespot_fts(rowid, title)
    SELECT id, title
    FROM suggestions
    WHERE id = new.suggestion_id;
END;
                    ",
                )?;
                Ok(())
            }
            22 => {
                // Drop and re-create the fakespot_fts table to remove the prefix index param
                tx.execute_batch(
                    "
DROP TABLE fakespot_fts;
CREATE VIRTUAL TABLE fakespot_fts USING FTS5(
  title,
  content='',
  contentless_delete=1,
  tokenize=\"porter unicode61 remove_diacritics 2 tokenchars '''-'\"
);
                    ",
                )?;
                Ok(())
            }
            23 => {
                // Drop all suggestions, then recreate the fakespot_custom_details table to add the
                // `keywords` and `product_type` fields.
                clear_database(tx)?;
                tx.execute_batch(
                    "
DROP TABLE fakespot_custom_details;
CREATE TABLE fakespot_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    fakespot_grade TEXT NOT NULL,
    product_id TEXT NOT NULL,
    keywords TEXT NOT NULL,
    product_type TEXT NOT NULL,
    rating REAL NOT NULL,
    total_reviews INTEGER NOT NULL,
    icon_id TEXT,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);
CREATE TRIGGER fakespot_ai AFTER INSERT ON fakespot_custom_details BEGIN
  INSERT INTO fakespot_fts(rowid, title)
    SELECT id, title
    FROM suggestions
    WHERE id = new.suggestion_id;
END;
                    ",
                )?;
                Ok(())
            }
            24 => {
                // Clear the database so that we re-ingest and populate the ingested_records table.
                clear_database(tx)?;
                tx.execute_batch(
                    "
CREATE TABLE rs_cache(
    collection TEXT PRIMARY KEY,
    data TEXT NOT NULL
) WITHOUT ROWID;
CREATE TABLE ingested_records(
    id TEXT,
    collection TEXT,
    type TEXT NOT NULL,
    last_modified INTEGER NOT NULL,
    PRIMARY KEY (id, collection)
) WITHOUT ROWID;
                    ",
                )?;
                Ok(())
            }
            25 => {
                // Create the exposure suggestions table and index.
                tx.execute_batch(
                    "
CREATE TABLE exposure_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    type TEXT NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);
CREATE INDEX exposure_custom_details_type ON exposure_custom_details(type);
                    ",
                )?;
                Ok(())
            }
            _ => Err(open_database::Error::IncompatibleVersion(version)),
        }
    }
}

/// Clears the database, removing all suggestions, icons, and metadata.
pub fn clear_database(db: &Connection) -> rusqlite::Result<()> {
    db.execute_batch(
        "
        DELETE FROM meta;
        DELETE FROM keywords;
        DELETE FROM full_keywords;
        DELETE FROM prefix_keywords;
        DELETE FROM suggestions;
        DELETE FROM icons;
        DELETE FROM yelp_subjects;
        DELETE FROM yelp_modifiers;
        DELETE FROM yelp_location_signs;
        DELETE FROM yelp_custom_details;
        ",
    )?;
    let table_exists: bool =
        db.query_one("SELECT EXISTS (SELECT 1 FROM sqlite_master WHERE name = 'fakespot_fts')")?;
    if table_exists {
        db.execute("DELETE FROM fakespot_fts", ())?;
    }
    Ok(())
}

#[cfg(test)]
mod test {
    use super::*;
    use sql_support::open_database::test_utils::MigratedDatabaseFile;

    // Snapshot of the v16 schema.  We use this to test that we can migrate from there to the
    // current schema.
    const V16_SCHEMA: &str = r#"
CREATE TABLE meta(
    key TEXT PRIMARY KEY,
    value NOT NULL
) WITHOUT ROWID;

CREATE TABLE keywords(
    keyword TEXT NOT NULL,
    suggestion_id INTEGER NOT NULL REFERENCES suggestions(id) ON DELETE CASCADE,
    full_keyword_id INTEGER NULL REFERENCES full_keywords(id) ON DELETE SET NULL,
    rank INTEGER NOT NULL,
    PRIMARY KEY (keyword, suggestion_id)
) WITHOUT ROWID;

-- full keywords are what we display to the user when a (partial) keyword matches
-- The FK to suggestion_id makes it so full keywords get deleted when the parent suggestion is deleted.
CREATE TABLE full_keywords(
    id INTEGER PRIMARY KEY,
    suggestion_id INTEGER NOT NULL REFERENCES suggestions(id) ON DELETE CASCADE,
    full_keyword TEXT NOT NULL
);

CREATE TABLE prefix_keywords(
    keyword_prefix TEXT NOT NULL,
    keyword_suffix TEXT NOT NULL DEFAULT '',
    confidence INTEGER NOT NULL DEFAULT 0,
    rank INTEGER NOT NULL,
    suggestion_id INTEGER NOT NULL REFERENCES suggestions(id) ON DELETE CASCADE,
    PRIMARY KEY (keyword_prefix, keyword_suffix, suggestion_id)
) WITHOUT ROWID;

CREATE UNIQUE INDEX keywords_suggestion_id_rank ON keywords(suggestion_id, rank);

CREATE TABLE suggestions(
    id INTEGER PRIMARY KEY,
    record_id TEXT NOT NULL,
    provider INTEGER NOT NULL,
    title TEXT NOT NULL,
    url TEXT NOT NULL,
    score REAL NOT NULL
);

CREATE TABLE amp_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    advertiser TEXT NOT NULL,
    block_id INTEGER NOT NULL,
    iab_category TEXT NOT NULL,
    impression_url TEXT NOT NULL,
    click_url TEXT NOT NULL,
    icon_id TEXT NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);

CREATE TABLE wikipedia_custom_details(
    suggestion_id INTEGER PRIMARY KEY REFERENCES suggestions(id) ON DELETE CASCADE,
    icon_id TEXT NOT NULL
);

CREATE TABLE amo_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    description TEXT NOT NULL,
    guid TEXT NOT NULL,
    icon_url TEXT NOT NULL,
    rating TEXT,
    number_of_ratings INTEGER NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);

CREATE INDEX suggestions_record_id ON suggestions(record_id);

CREATE TABLE icons(
    id TEXT PRIMARY KEY,
    data BLOB NOT NULL,
    mimetype TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE yelp_subjects(
    keyword TEXT PRIMARY KEY,
    record_id TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE yelp_modifiers(
    type INTEGER NOT NULL,
    keyword TEXT NOT NULL,
    record_id TEXT NOT NULL,
    PRIMARY KEY (type, keyword)
) WITHOUT ROWID;

CREATE TABLE yelp_location_signs(
    keyword TEXT PRIMARY KEY,
    need_location INTEGER NOT NULL,
    record_id TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE yelp_custom_details(
    icon_id TEXT PRIMARY KEY,
    score REAL NOT NULL,
    record_id TEXT NOT NULL
) WITHOUT ROWID;

CREATE TABLE mdn_custom_details(
    suggestion_id INTEGER PRIMARY KEY,
    description TEXT NOT NULL,
    FOREIGN KEY(suggestion_id) REFERENCES suggestions(id) ON DELETE CASCADE
);

PRAGMA user_version=16;
"#;

    /// Test running all schema upgrades from V16, which was the first schema with a "real"
    /// migration.
    ///
    /// If an upgrade fails, then this test will fail with a panic.
    #[test]
    fn test_all_upgrades() {
        let db_file =
            MigratedDatabaseFile::new(SuggestConnectionInitializer::default(), V16_SCHEMA);
        db_file.run_all_upgrades();
        db_file.assert_schema_matches_new_database();
    }
}
