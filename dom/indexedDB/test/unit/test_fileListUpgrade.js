/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testGenerator */
var testGenerator = testSteps();

function* testSteps() {
  /**
   * High level description: This is a synthetic test whose main aim is to
   * ensure that DeserializeUpgradeValueHelper is safe with the FileList type
   * in its future use cases, and in the unlikely case that someone traverses
   * this code path with an artificially constructed profile.
   *
   * The packaged profile of filelistUpgrade_profile.zip was manufactured in
   * the following way:
   *
   * 1. IDBFactory methods OpenForPrincipal was exposed by removing the
   * ChromeOnly attribute from the IDBFactory.webidl file and making the
   * corresponding changes to the header and source files.
   * 1. mochitest test_file_filelist.html was converted to an xpcshell-test
   * which from here on is referred to as test_file_filelist.js
   * 1. test_file_filelist.js test was modified to create a principal for
   * example.com the same way as dom/push/test/xpcshell/test_service_child.js
   * 1. test_file_filelist.js test was modified to open the databases using
   * the openForPrincipal method, with the principal above
   * 1. test_file_filelist.js test was executed and the related
   * xpcshell profile was saved
   * 1. this whole play with the principals is necessary to make sure that the
   * resulting database is not encrypted, and can be queried with a normal
   * sqlite3 client
   * 1. file dom/indexedDB/SchemaUpgrades.cpp was modified to expect the
   * current database version to be 19 and stop the upgrades at this version
   * 1. additionally, the function UpgradeKeyFunction of
   * UpgradeSchemaFrom17_0To18_0Helper was modified to copy the aSource value to
   * aDestination, set aTagOffset to zero and return with NS_OK when the
   * aSource value equals 48
   * 1. test_schema18upgrade.js was modified to stop after the database upgrade
   * and executed. The related xpcshell profile with version 19 was saved.
   * 1. the version 19 database was opened with sqlite3 client and the other
   * profile with a filelist was attached to it as "future"
   * 1. values (2, 0, "FileLists", "key") and
   * (3, 0, "Other FileLists", "key") were inserted to table object_store
   * 1. the .schema command was executed to record the current schema
   * 1. object_data_insert_trigger was dropped (temporarily)
   * 1. the following query was used to insert all data from the attached
   * database to the old version:
   *   INSERT INTO object_data (
   *     object_store_id,
   *     key,
   *     index_data_values,
   *     file_ids,
   *     data)
   *   SELECT
   *     (CASE WHEN (ROW_NUMBER() OVER () <= 2) THEN 2 ELSE 3 END),
   *     key,
   *     NULL,
   *     file_ids,
   *     data
   *   FROM future.object_data;
   * 1. everything from future.file is inserted to file
   * 1. the following query was used to update the database name:
   *    INSERT INTO database
   *    SELECT
   *      'fileListUpgrade',
   *      origin,
   *      version,
   *      last_vacuum_time,
   *      last_analyze_time,
   *      last_vacuum_size
   *    FROM database WHERE name = 'schema18upgrade';
   * 1. the row with name 'schema18upgrade' was deleted from database
   * 1. the sqlite3 client was closed
   * 1. the filename that is expected for this test was found out with the
   * GetFileName method of dbFileUrl in the beginning of CreateStorageConnection
   * function in dom/indexedDB/ActorsParent.cpp
   * 1. under the idb-directory under the profile/storage path, the
   * .sqlite database file and the directory with a matching name having
   * a .files extension were renamed to have the expected name with the same
   * extensions
   * 1. the file '1' and the journal firectory from the other, filelist
   * containing profile, for the example.com principal, were copied under the
   * just renamed .files directory
   * 1. lastly, the storage/ folder and storage.sqlite were recursively zipped
   * into an archive (note: zip -r on wsl did not produce a readable archive,
   * the artifact was created with the windows file manager compression tool)
   */
  const testName = "fileListUpgrade";

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  info("Installing profile");

  installPackagedProfile(testName + "_profile");

  info("Opening existing database with a filelist");

  let request = indexedDB.open(testName);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;
  info("Database version " + event.target.result.version);

  let db = event.target.result;

  is(db.version, 3, "Correct db version");

  let transaction = db.transaction(["FileLists"]);
  transaction.oncomplete = grabEventAndContinueHandler;

  let objectStore = transaction.objectStore("FileLists");

  request = objectStore.openCursor();
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  var cursorItems = 0;
  const expectedValues = new Set([
    '{"key":"A","idx":0,"fileList":{"0":{},"1":{},"2":{}},"blob":{}}',
    '{"key":"C","idx":2,"fileList":{"0":{},"1":{},"2":{}},"blob":{}}',
  ]);
  request.onsuccess = event => {
    let cursor = event.target.result;
    if (cursor) {
      cursorItems = cursorItems + 1;
      const actualValue = JSON.stringify(cursor.value);
      ok(expectedValues.has(actualValue), "Is value as expected?");
      cursor.continue();
    } else {
      testGenerator.next();
    }
  };
  yield undefined;
  is(cursorItems, expectedValues.size, "Did we get expected number of items?");

  finishTest();
  yield undefined;
}
