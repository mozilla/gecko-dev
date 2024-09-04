/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  await setupPlacesDatabase("places_v77.sqlite");
  const faviconsPath = await setupPlacesDatabase(
    "favicons_v41.sqlite",
    "favicons.sqlite"
  );

  const faviconsDb = await Sqlite.openConnection({ path: faviconsPath });

  await faviconsDb.execute(`
    INSERT INTO moz_icons (id, icon_url, fixed_icon_url_hash, width, root, expire_ms)
    VALUES
      (1, 'http://example.com/icon.png', 12345, 16, 0, 0)
  `);

  await faviconsDb.close();
});

add_task(async function database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_UPGRADED
  );

  const db = await PlacesUtils.promiseDBConnection();
  Assert.equal(await db.getSchemaVersion(), CURRENT_SCHEMA_VERSION);
});

add_task(async function moz_icons() {
  await PlacesUtils.withConnectionWrapper("test_sqlite_migration", async db => {
    const rows = await db.execute("SELECT * FROM moz_icons WHERE id=1");

    Assert.equal(rows.length, 1);
    Assert.equal(rows[0].getResultByName("id"), 1);
    Assert.equal(rows[0].getResultByName("flags"), 0);
  });
});
