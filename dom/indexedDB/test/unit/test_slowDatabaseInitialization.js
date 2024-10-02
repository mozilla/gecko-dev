/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { PrincipalUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/PrincipalUtils.sys.mjs"
);
const { QuotaUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/QuotaUtils.sys.mjs"
);
const { IndexedDBUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/indexedDB/test/modules/IndexedDBUtils.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

async function testNewDatabase() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");
  const name = "test_slowStorageInitialization.js";

  info("Testing origin clearing requested after starting database work");

  info("Starting database opening");

  const openPromise = (function () {
    const request = indexedDB.openForPrincipal(principal, name);
    const promise = IndexedDBUtils.requestFinished(request);
    return promise;
  })();

  info("Waiting for database work to start");

  await TestUtils.topicObserved("IndexedDB::DatabaseWorkStarted");

  info("Starting origin clearing");

  const clearPromise = (async function () {
    const request = Services.qms.clearStoragesForPrincipal(principal);
    const promise = QuotaUtils.requestFinished(request);
    return promise;
  })();

  info("Waiting for database to finish opening");

  try {
    await openPromise;
    ok(false, "Should have thrown");
  } catch (e) {
    ok(true, "Should have thrown");
    Assert.strictEqual(e.name, "UnknownError", "Threw right result code");
  }

  info("Waiting for origin to finish clearing");

  await clearPromise;
}

async function testExistingDatabase() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");
  const name = "test_slowStorageInitialization.js";
  const objectStoreName = "foo";

  info("Testing origin clearing requested after starting database work");

  info("Ensuring a pre-existing database");

  {
    const request = indexedDB.openForPrincipal(principal, name);
    request.onupgradeneeded = function (event) {
      const database = event.target.result;
      database.createObjectStore(objectStoreName);
    };
    const promise = IndexedDBUtils.requestFinished(request);
    const database = await promise;
    database.close();
  }

  info("Starting database opening");

  const openPromise = (function () {
    const request = indexedDB.openForPrincipal(principal, name);
    const promise = IndexedDBUtils.requestFinished(request);
    return promise;
  })();

  info("Waiting for database work to start");

  await TestUtils.topicObserved("IndexedDB::DatabaseWorkStarted");

  info("Starting origin clearing");

  const clearPromise = (async function () {
    const request = Services.qms.clearStoragesForPrincipal(principal);
    const promise = QuotaUtils.requestFinished(request);
    return promise;
  })();

  info("Waiting for database to finish opening");

  try {
    await openPromise;
    ok(false, "Should have thrown");
  } catch (e) {
    ok(true, "Should have thrown");
    Assert.strictEqual(e.name, "AbortError", "Threw right result code");
  }

  info("Waiting for origin to finish clearing");

  await clearPromise;
}

/* exported testSteps */
async function testSteps() {
  add_task(
    {
      pref_set: [
        ["dom.indexedDB.databaseInitialization.pauseOnIOThreadMs", 2000],
      ],
    },
    testNewDatabase
  );

  add_task(
    {
      pref_set: [
        ["dom.indexedDB.databaseInitialization.pauseOnIOThreadMs", 2000],
      ],
    },
    testExistingDatabase
  );
}
