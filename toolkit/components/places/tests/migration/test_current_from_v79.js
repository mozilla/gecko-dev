/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const indices = [
  "moz_newtab_story_click_idx_newtab_click_timestamp",
  "moz_newtab_story_click_idx_newtab_impression_timestamp",
];

add_task(async function setup() {
  let path = await setupPlacesDatabase("places_v79.sqlite");
  let db = await Sqlite.openConnection({ path });
  let rows = await db.execute(
    "SELECT count(*) FROM sqlite_master WHERE name IN(?,?)",
    indices
  );
  Assert.equal(rows[0].getResultByIndex(0), 2, "Check indices are present");
  await db.close();
});

add_task(async function database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_UPGRADED
  );

  const db = await PlacesUtils.promiseDBConnection();
  Assert.equal(await db.getSchemaVersion(), CURRENT_SCHEMA_VERSION);

  let rows = await db.execute(
    "SELECT count(*) FROM sqlite_master WHERE name IN (?,?)",
    indices
  );
  Assert.equal(rows[0].getResultByIndex(0), 0, "Check indices were removed");
});
