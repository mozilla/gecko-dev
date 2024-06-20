/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

/* exported testGenerator */
var testGenerator = testSteps();

function* testSteps() {
  let uri = Services.io.newURI("https://www.example.com");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  info("Setting permissions");

  Services.perms.addFromPrincipal(
    principal,
    "indexedDB",
    Ci.nsIPermissionManager.ALLOW_ACTION
  );

  info("Creating databases");

  // Keep at least one database open.
  let req = indexedDB.open("foo-a", 1);
  req.onerror = errorHandler;
  req.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  // Keep at least one factory operation alive by deleting a database that is
  // stil open.
  req = indexedDB.open("foo-b", 1);
  req.onerror = errorHandler;
  req.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  indexedDB.deleteDatabase("foo-b");

  // Create a database which we will later try to open while maintenance is
  // performed.
  req = indexedDB.open("foo-c", 1);
  req.onerror = errorHandler;
  req.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  let dbC = event.target.result;
  dbC.close();

  let dbCount = 0;

  for (let persistence of ["persistent", "temporary", "default"]) {
    for (let i = 1; i <= 5; i++) {
      let dbName = "foo-" + i;
      let dbPersistence = persistence;
      let req = indexedDB.openForPrincipal(principal, dbName, {
        version: 1,
        storage: dbPersistence,
      });
      req.onerror = event => {
        if (dbPersistence != "persistent") {
          errorHandler(event);
          return;
        }

        // Explicit persistence is currently blocked on mobile.
        info(
          "Failed to create persistent database '" +
            dbPersistence +
            "/" +
            dbName +
            "', hopefully this is on mobile!"
        );

        event.preventDefault();

        if (!--dbCount) {
          continueToNextStep();
        }
      };
      req.onupgradeneeded = event => {
        let db = event.target.result;
        let objectStore = db.createObjectStore("foo");

        // Add lots of data...
        for (let j = 0; j < 100; j++) {
          objectStore.add("abcdefghijklmnopqrstuvwxyz0123456789", j);
        }

        // And then clear it so that maintenance has some space to reclaim.
        objectStore.clear();
      };
      req.onsuccess = event => {
        let db = event.target.result;
        ok(db, "Created database '" + dbPersistence + "/" + dbName + "'");

        db.close();

        if (!--dbCount) {
          continueToNextStep();
        }
      };
      dbCount++;
    }
  }
  yield undefined;

  info("Getting usage before maintenance");

  let usageBeforeMaintenance;

  Services.qms.getUsageForPrincipal(principal, request => {
    let usage = request.result.usage;
    ok(usage > 0, "Usage is non-zero");
    usageBeforeMaintenance = usage;
    continueToNextStep();
  });
  yield undefined;

  info("Starting idle maintenance");

  const indexedDatabaseManager = Cc[
    "@mozilla.org/dom/indexeddb/manager;1"
  ].getService(Ci.nsIIndexedDatabaseManager);

  const maintenancePromise = indexedDatabaseManager.doMaintenance();

  info("Opening database while maintenance is performed");

  req = indexedDB.open("foo-c", 1);
  req.onerror = errorHandler;
  req.onsuccess = grabEventAndContinueHandler;
  yield undefined;

  info("Waiting for maintenance to finish");

  maintenancePromise.then(continueToNextStep);
  yield undefined;

  info("Getting usage after maintenance");

  let usageAfterMaintenance;

  Services.qms.getUsageForPrincipal(principal, request => {
    let usage = request.result.usage;
    ok(usage > 0, "Usage is non-zero");
    usageAfterMaintenance = usage;
    continueToNextStep();
  });
  yield undefined;

  info(
    "Usage before: " +
      usageBeforeMaintenance +
      ". " +
      "Usage after: " +
      usageAfterMaintenance
  );

  ok(
    usageAfterMaintenance <= usageBeforeMaintenance,
    "Maintenance decreased file sizes or left them the same"
  );

  finishTest();
  yield undefined;
}
