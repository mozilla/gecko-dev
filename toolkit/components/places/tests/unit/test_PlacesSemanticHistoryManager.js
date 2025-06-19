/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
  getPlacesSemanticHistoryManager:
    "resource://gre/modules/PlacesSemanticHistoryManager.sys.mjs",
});

// Must be divisible by 8.
const EMBEDDING_SIZE = 16;

function approxEqual(a, b, tolerance = 1e-6) {
  return Math.abs(a - b) < tolerance;
}

function createPlacesSemanticHistoryManager() {
  return getPlacesSemanticHistoryManager(
    {
      embeddingSize: EMBEDDING_SIZE,
      rowLimit: 10,
    },
    true
  );
}

class MockMLEngine {
  async run(request) {
    const texts = request.args[0];
    return texts.map(text => {
      if (typeof text !== "string" || text.trim() === "") {
        throw new Error("Invalid input: text must be a non-empty string");
      }
      // Return a mock embedding vector (e.g., an array of zeros)
      return Array(EMBEDDING_SIZE).fill(0);
    });
  }
}

add_setup(async function () {
  Services.fog.initializeFOG();
});

add_task(async function test_tensorToBindable() {
  const semanticManager = createPlacesSemanticHistoryManager();
  let tensor = [0.3, 0.3, 0.3, 0.3];
  let bindable = semanticManager.tensorToBindable(tensor);
  Assert.equal(
    Object.prototype.toString.call(bindable),
    "[object Uint8ClampedArray]",
    "tensorToBindable should return a Uint8ClampedArray"
  );
  let floatArray = new Float32Array(bindable.buffer);
  Assert.equal(
    floatArray.length,
    4,
    "Float32Array should have the same length as tensor"
  );
  for (let i = 0; i < 4; i++) {
    Assert.ok(
      approxEqual(floatArray[i], tensor[i]),
      "Element " +
        i +
        " matches expected value within tolerance. " +
        "Expected: " +
        tensor[i] +
        ", got: " +
        floatArray[i]
    );
  }
});

add_task(async function test_shutdown_no_error() {
  const semanticManager = createPlacesSemanticHistoryManager();

  sinon.stub(semanticManager.semanticDB, "closeConnection").resolves();
  await semanticManager.shutdown();

  Assert.ok(
    semanticManager.semanticDB.closeConnection.called,
    "Connection close() should be invoked"
  );
  sinon.reset();
});

add_task(async function test_canUseSemanticSearch_all_conditions_met() {
  const semanticManager = createPlacesSemanticHistoryManager();

  Services.prefs.setBoolPref("browser.ml.enable", true);
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", true);

  semanticManager.qualifiedForSemanticSearch = true;
  semanticManager.enoughEntries = true;

  Assert.ok(
    semanticManager.canUseSemanticSearch,
    "Semantic search should be enabled when all conditions met."
  );
});

add_task(async function test_canUseSemanticSearch_ml_disabled() {
  const semanticManager = createPlacesSemanticHistoryManager();

  Services.prefs.setBoolPref("browser.ml.enable", false);
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", true);

  semanticManager.qualifiedForSemanticSearch = true;
  semanticManager.enoughEntries = true;

  Assert.ok(
    !semanticManager.canUseSemanticSearch,
    "Semantic search should be disabled when ml disabled."
  );
});

add_task(async function test_canUseSemanticSearch_featureGate_disabled() {
  const semanticManager = createPlacesSemanticHistoryManager();

  Services.prefs.setBoolPref("browser.ml.enable", true);
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", false);

  semanticManager.qualifiedForSemanticSearch = true;
  semanticManager.enoughEntries = true;

  Assert.ok(
    !semanticManager.canUseSemanticSearch,
    "Semantic search should be disabled when featureGate disabled."
  );
});

add_task(async function test_canUseSemanticSearch_not_qualified() {
  const semanticManager = createPlacesSemanticHistoryManager();

  Services.prefs.setBoolPref("browser.ml.enable", true);
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", true);

  semanticManager.qualifiedForSemanticSearch = false;
  semanticManager.enoughEntries = true;

  Assert.ok(
    !semanticManager.canUseSemanticSearch,
    "Semantic search should be disabled when not qualified."
  );
});

add_task(async function test_removeDatabaseFilesOnDisable() {
  // Ensure Places has been initialized.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_CREATE,
    "Places database should be initialized."
  );
  let semanticManager = createPlacesSemanticHistoryManager();
  await semanticManager.getConnection();

  Assert.ok(await IOUtils.exists(semanticManager.semanticDB.databaseFilePath));
  Assert.ok(
    await IOUtils.exists(semanticManager.semanticDB.databaseFilePath + "-wal")
  );

  Services.fog.testResetFOG();
  await PlacesDBUtils.telemetry();
  Assert.equal(
    Glean.places.databaseSemanticHistoryFilesize.testGetValue().count,
    1,
    "Check for file size being collected"
  );

  await semanticManager.shutdown();

  // Create a new instance of the manager after disabling the feature.
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", false);
  semanticManager = createPlacesSemanticHistoryManager();

  Assert.ok(
    !semanticManager.canUseSemanticSearch,
    "Semantic search should be disabled."
  );

  await TestUtils.waitForCondition(async () => {
    return (
      !(await IOUtils.exists(semanticManager.semanticDB.databaseFilePath)) &&
      !(await IOUtils.exists(
        semanticManager.semanticDB.databaseFilePath + "-wal"
      ))
    );
  }, "Wait for database files to be removed");
});

add_task(async function test_removeDatabaseFilesOnStartup() {
  // Ensure Places has been initialized.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_CREATE,
    "Places database should be initialized."
  );

  Services.prefs.setBoolPref("places.semanticHistory.featureGate", true);
  let semanticManager = createPlacesSemanticHistoryManager();

  Assert.ok(
    semanticManager.canUseSemanticSearch,
    "Semantic search should be enabled."
  );
  await semanticManager.getConnection();

  Assert.ok(await IOUtils.exists(semanticManager.semanticDB.databaseFilePath));
  Assert.ok(
    await IOUtils.exists(semanticManager.semanticDB.databaseFilePath + "-wal")
  );
  await semanticManager.shutdown();

  // Create a new instance of the manager after setting the pref.
  Services.prefs.setBoolPref("places.semanticHistory.removeOnStartup", true);
  semanticManager = createPlacesSemanticHistoryManager();

  Assert.ok(
    !Services.prefs.getBoolPref(
      "places.semanticHistory.removeOnStartup",
      false
    ),
    "Pref should have been reset."
  );
});

add_task(async function test_chunksTelemetry() {
  await PlacesTestUtils.addVisits([
    { url: "https://test1.moz.com/", title: "test 1" },
    { url: "https://test2.moz.com/", title: "test 2" },
  ]);

  Services.fog.testResetFOG();

  Assert.strictEqual(
    Glean.places.semanticHistoryFindChunksTime.testGetValue(),
    null,
    "No value initially"
  );
  Assert.strictEqual(
    Glean.places.semanticHistoryChunkCalculateTime.testGetValue(),
    null,
    "No value initially"
  );
  Assert.strictEqual(
    Glean.places.semanticHistoryMaxChunksCount.testGetValue(),
    null,
    "No value initially"
  );
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", true);

  let semanticManager = createPlacesSemanticHistoryManager();
  // Ensure only one task execution for measuremant purposes.
  semanticManager.setDeferredTaskIntervalForTests(3000);
  await semanticManager.getConnection();
  semanticManager.embedder.setEngine(new MockMLEngine());
  await TestUtils.topicObserved(
    "places-semantichistorymanager-update-complete"
  );

  Assert.equal(
    Glean.places.semanticHistoryFindChunksTime.testGetValue().count,
    1
  );
  Assert.greater(
    Glean.places.semanticHistoryFindChunksTime.testGetValue().sum,
    0
  );

  Assert.equal(
    Glean.places.semanticHistoryChunkCalculateTime.testGetValue().count,
    1
  );
  Assert.greater(
    Glean.places.semanticHistoryChunkCalculateTime.testGetValue().sum,
    0
  );

  Assert.equal(Glean.places.semanticHistoryMaxChunksCount.testGetValue(), 1);

  await semanticManager.shutdown();
});

add_task(async function test_duplicate_urlhash() {
  const urls = [
    { url: "https://test1.moz.com/", title: "test 1" },
    { url: "https://test2.moz.com/", title: "test 2" },
    { url: "https://test3.moz.com/", title: "test 3" },
  ];
  await PlacesTestUtils.addVisits(urls);
  // We're manually editing the database to create a duplicate url hash.
  const urlHash = PlacesUtils.history.hashURL(urls[0].url);
  await PlacesUtils.withConnectionWrapper("test", async db => {
    await db.execute(
      `UPDATE moz_places SET url_hash = :urlHash WHERE url = :url`,
      { urlHash, url: urls[1].url }
    );
  });

  let semanticManager = createPlacesSemanticHistoryManager();
  // Ensure only one task execution for measuremant purposes.
  semanticManager.setDeferredTaskIntervalForTests(3000);
  let conn = await semanticManager.getConnection();
  semanticManager.embedder.setEngine(new MockMLEngine());
  await TestUtils.topicObserved(
    "places-semantichistorymanager-update-complete"
  );

  // Check the update continued despite the duplicate url hash.
  let rows = await conn.execute(`SELECT url_hash FROM vec_history_mapping`);
  Assert.equal(rows.length, 2, "There should be two entries");
  Assert.equal(
    rows[0].getResultByName("url_hash"),
    PlacesUtils.history.hashURL(urls[0].url),
    "First URL hash should match"
  );
  Assert.equal(
    rows[1].getResultByName("url_hash"),
    PlacesUtils.history.hashURL(urls[2].url),
    "Third URL hash should match"
  );
  await semanticManager.shutdown();
});
