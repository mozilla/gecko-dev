/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PlacesSemanticHistoryManager } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesSemanticHistoryManager.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

function approxEqual(a, b, tolerance = 1e-6) {
  return Math.abs(a - b) < tolerance;
}

function createPlacesSemanticHistoryManager() {
  return new PlacesSemanticHistoryManager({
    embeddingSize: 4,
    rowLimit: 10,
    samplingAttrib: "frecency",
    changeThresholdCount: 3,
    distanceThreshold: 0.75,
    testFlag: true,
  });
}

add_task(async function test_tensorToBindable() {
  let manager = new createPlacesSemanticHistoryManager();
  let tensor = [0.3, 0.3, 0.3, 0.3];
  let bindable = manager.tensorToBindable(tensor);
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
  let manager = new createPlacesSemanticHistoryManager();

  sinon.stub(manager.semanticDB, "closeConnection").resolves();
  await manager.shutdown();

  Assert.ok(
    manager.semanticDB.closeConnection.called,
    "Connection close() should be invoked"
  );
  sinon.reset();
});

add_task(async function test_canUseSemanticSearch_all_conditions_met() {
  let manager = new createPlacesSemanticHistoryManager();

  Services.prefs.setBoolPref("browser.ml.enable", true);
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", true);

  manager.qualifiedForSemanticSearch = true;
  manager.enoughEntries = true;

  Assert.ok(
    manager.canUseSemanticSearch,
    "Semantic search should be enabled when all conditions met."
  );
});

add_task(async function test_canUseSemanticSearch_ml_disabled() {
  let manager = new createPlacesSemanticHistoryManager();

  Services.prefs.setBoolPref("browser.ml.enable", false);
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", true);

  manager.qualifiedForSemanticSearch = true;
  manager.enoughEntries = true;

  Assert.ok(
    !manager.canUseSemanticSearch,
    "Semantic search should be disabled when ml disabled."
  );
});

add_task(async function test_canUseSemanticSearch_featureGate_disabled() {
  let manager = new createPlacesSemanticHistoryManager();

  Services.prefs.setBoolPref("browser.ml.enable", true);
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", false);

  manager.qualifiedForSemanticSearch = true;
  manager.enoughEntries = true;

  Assert.ok(
    !manager.canUseSemanticSearch,
    "Semantic search should be disabled when featureGate disabled."
  );
});

add_task(async function test_canUseSemanticSearch_not_qualified() {
  let manager = new createPlacesSemanticHistoryManager();

  Services.prefs.setBoolPref("browser.ml.enable", true);
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", true);

  manager.qualifiedForSemanticSearch = false;
  manager.enoughEntries = true;

  Assert.ok(
    !manager.canUseSemanticSearch,
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
  let manager = new createPlacesSemanticHistoryManager();
  await manager.getConnection();

  Assert.ok(await IOUtils.exists(manager.semanticDB.databaseFilePath));
  Assert.ok(await IOUtils.exists(manager.semanticDB.databaseFilePath + "-wal"));

  await manager.shutdown();

  // Create a new instance of the manager after disabling the feature.
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", false);
  manager = new createPlacesSemanticHistoryManager();

  Assert.ok(
    !manager.canUseSemanticSearch,
    "Semantic search should be disabled."
  );

  await TestUtils.waitForCondition(async () => {
    return (
      !(await IOUtils.exists(manager.semanticDB.databaseFilePath)) &&
      !(await IOUtils.exists(manager.semanticDB.databaseFilePath + "-wal"))
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
  let manager = new createPlacesSemanticHistoryManager();
  Assert.ok(manager.canUseSemanticSearch, "Semantic search should be enabled.");
  await manager.getConnection();

  Assert.ok(await IOUtils.exists(manager.semanticDB.databaseFilePath));
  Assert.ok(await IOUtils.exists(manager.semanticDB.databaseFilePath + "-wal"));

  await manager.shutdown();

  // Create a new instance of the manager after setting the pref.
  Services.prefs.setBoolPref("places.semanticHistory.removeOnStartup", true);

  manager = new createPlacesSemanticHistoryManager();

  Assert.ok(
    !Services.prefs.getBoolPref(
      "places.semanticHistory.removeOnStartup",
      false
    ),
    "Pref should have been reset."
  );
});
