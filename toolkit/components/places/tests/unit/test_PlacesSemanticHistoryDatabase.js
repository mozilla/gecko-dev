/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PlacesSemanticHistoryDatabase } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesSemanticHistoryDatabase.sys.mjs"
);

add_setup(async function () {
  // Initialize Places.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_CREATE,
    "Places initialized."
  );
});

add_task(async function test_check_schema() {
  let db = new PlacesSemanticHistoryDatabase({
    embeddingSize: 4,
    fileName: "places_semantic.sqlite",
  });

  let conn = await db.getConnection();
  Assert.equal(
    await conn.getSchemaVersion(),
    db.currentSchemaVersion,
    "Schema version should match."
  );
  await db.closeConnection();
  await db.removeDatabaseFiles();
});

add_task(async function test_replace_on_downgrade() {
  let db = new PlacesSemanticHistoryDatabase({
    embeddingSize: 4,
    fileName: "places_semantic.sqlite",
  });

  let conn = await db.getConnection();
  let originalSchemaVersion = db.currentSchemaVersion;
  await db.setCurrentSchemaVersionForTests(originalSchemaVersion + 1);
  await db.closeConnection();
  await db.setCurrentSchemaVersionForTests(originalSchemaVersion);
  conn = await db.getConnection();
  Assert.equal(
    await conn.getSchemaVersion(),
    db.currentSchemaVersion,
    "Schema version should have been reset."
  );
  await db.closeConnection();
  await db.removeDatabaseFiles();
});

add_task(async function test_broken_schema() {
  let db = new PlacesSemanticHistoryDatabase({
    embeddingSize: 4,
    fileName: "places_semantic.sqlite",
  });

  let conn = await db.getConnection();
  await conn.execute("DROP TABLE vec_history_mapping");
  await db.closeConnection();

  conn = await db.getConnection();
  let rows = await conn.execute("SELECT COUNT(*) FROM vec_history_mapping");
  Assert.equal(
    rows[0].getResultByIndex(0),
    0,
    "Schema should have been reset."
  );
  await db.closeConnection();
  await db.removeDatabaseFiles();
});

add_task(async function test_corruptdb() {
  let db = new PlacesSemanticHistoryDatabase({
    embeddingSize: 4,
    fileName: "places_semantic.sqlite",
  });
  // Move a corrupt database file in place.
  await IOUtils.copy(
    do_get_file("../maintenance/corruptDB.sqlite").path,
    db.databaseFilePath
  );
  let conn = await db.getConnection();
  Assert.equal(
    await conn.getSchemaVersion(),
    db.currentSchemaVersion,
    "Schema version should have been set."
  );
  await db.closeConnection();
  await db.removeDatabaseFiles();
});

add_task(async function test_healthydb() {
  let db = new PlacesSemanticHistoryDatabase({
    embeddingSize: 4,
    fileName: "places_semantic.sqlite",
  });
  await db.getConnection();
  await db.closeConnection();
  // Check database creation time won't change when reopening, as that would
  // indicate the database file was replaced.
  let creationTime = (await IOUtils.stat(db.databaseFilePath)).creationTime;
  db = new PlacesSemanticHistoryDatabase({
    embeddingSize: 4,
    fileName: "places_semantic.sqlite",
  });
  await db.getConnection();
  await db.closeConnection();
  Assert.equal(
    creationTime,
    (await IOUtils.stat(db.databaseFilePath)).creationTime,
    "Database creation time should not change."
  );
});
