/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(unknown_lints)]
#![warn(rust_2018_idioms)]

//! A crate with various sql/sqlcipher helpers.

mod conn_ext;

// XXX - temporarily disable our debug_tools, to avoid pulling in:
// prettytable-rs = { version = "0.10", optional = true }
// while vendoring into m-c :(
pub mod debug_tools {
    pub fn define_debug_functions(_c: &rusqlite::Connection) -> rusqlite::Result<()> {
        Ok(())
    }
}

mod each_chunk;
mod lazy;
mod maybe_cached;
pub mod open_database;
mod repeat;

pub use conn_ext::*;
pub use each_chunk::*;
pub use lazy::*;
pub use maybe_cached::*;
pub use repeat::*;

/// In PRAGMA foo='bar', `'bar'` must be a constant string (it cannot be a
/// bound parameter), so we need to escape manually. According to
/// <https://www.sqlite.org/faq.html>, the only character that must be escaped is
/// the single quote, which is escaped by placing two single quotes in a row.
pub fn escape_string_for_pragma(s: &str) -> String {
    s.replace('\'', "''")
}

/// Default SQLite pragmas
///
/// Most components should just stick to these defaults.
pub fn setup_sqlite_defaults(conn: &rusqlite::Connection) -> rusqlite::Result<()> {
    conn.execute_batch(
        "
        PRAGMA temp_store = 2;
        PRAGMA journal_mode = WAL;
        ",
    )?;
    let page_size: usize = conn.query_row("PRAGMA page_size", (), |row| row.get(0))?;
    // Aim to checkpoint at 512Kb
    let target_checkpoint_size = 2usize.pow(19);
    // Truncate the journal if it more than 3x larger than the target size
    let journal_size_limit = target_checkpoint_size * 3;
    conn.execute_batch(&format!(
        "
        PRAGMA wal_autocheckpoint = {};
        PRAGMA journal_size_limit = {};
        ",
        target_checkpoint_size / page_size,
        journal_size_limit,
    ))?;

    Ok(())
}

#[cfg(test)]
mod test {
    use super::*;
    #[test]
    fn test_escape_string_for_pragma() {
        assert_eq!(escape_string_for_pragma("foobar"), "foobar");
        assert_eq!(escape_string_for_pragma("'foo'bar'"), "''foo''bar''");
        assert_eq!(escape_string_for_pragma("''"), "''''");
    }

    #[test]
    fn test_sqlite_defaults() {
        let conn = rusqlite::Connection::open_in_memory().unwrap();
        // Simulate a default page size,
        // On Mobile, these are set by the OS.  On Desktop, these are set by the build system when
        // we compile SQLite.
        conn.execute("PRAGMA page_size = 8192", ()).unwrap();
        setup_sqlite_defaults(&conn).unwrap();
        let autocheckpoint: usize = conn
            .query_row("PRAGMA wal_autocheckpoint", (), |row| row.get(0))
            .unwrap();
        // We should aim to auto-checkpoint at 512kb, which is 64 pages when the page size is 8k
        assert_eq!(autocheckpoint, 64);
        // We could also check the journal size limit, but that's harder to query with a pragma.
        // If we go the math right once, we should get it for the other case.
    }
}
