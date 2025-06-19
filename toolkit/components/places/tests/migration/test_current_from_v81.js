/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const indices = [
  "moz_newtab_shortcuts_interaction_timestampindex",
  "moz_newtab_shortcuts_interaction_placeidindex",
];

add_task(async function setup() {
  // setting up the pre-migration database
  let path = await setupPlacesDatabase("places_v81.sqlite");
  let db = await Sqlite.openConnection({ path });

  // optional sanity check to make sure indices are not present before migration
  let rows = await db.execute(
    "SELECT count(*) FROM sqlite_master WHERE name IN (?, ?)",
    indices
  );
  Assert.equal(
    rows[0].getResultByIndex(0),
    0,
    "Pre-migration: indices should not exist"
  );

  await db.close();
});

add_task(async function database_is_valid() {
  // trigger migration
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_UPGRADED
  );

  const db = await PlacesUtils.promiseDBConnection();
  Assert.equal(await db.getSchemaVersion(), CURRENT_SCHEMA_VERSION);

  let rows = await db.execute(
    "SELECT count(*) FROM sqlite_master WHERE name IN (?, ?)",
    indices
  );
  Assert.equal(
    rows[0].getResultByIndex(0),
    2,
    "Post-migration: indices were created"
  );
});
