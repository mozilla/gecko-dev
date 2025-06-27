/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::ConnExt;
use rusqlite::{Connection, Result};

/// Run maintenance on the DB
///
/// `run_maintenance()` is intended to be run during idle time and will take steps to clean up /
/// shrink the database.
pub fn run_maintenance(conn: &Connection) -> Result<()> {
    vacuum(conn)?;
    conn.execute_one("PRAGMA optimize")?;
    conn.execute_one("PRAGMA wal_checkpoint(PASSIVE)")?;
    Ok(())
}

/// Run vacuum on the DB
fn vacuum(conn: &Connection) -> Result<()> {
    let auto_vacuum_setting: u32 = conn.query_one("PRAGMA auto_vacuum")?;
    if auto_vacuum_setting == 2 {
        // Ideally, we run an incremental vacuum to delete 2 pages
        conn.execute_one("PRAGMA incremental_vacuum(2)")?;
    } else {
        // If auto_vacuum=incremental isn't set, configure it and run a full vacuum.
        error_support::warn!(
            "run_maintenance_vacuum: Need to run a full vacuum to set auto_vacuum=incremental"
        );
        conn.execute_one("PRAGMA auto_vacuum=incremental")?;
        conn.execute_one("VACUUM")?;
    }
    Ok(())
}
