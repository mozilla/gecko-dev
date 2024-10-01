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

/* exported testSteps */
async function testSteps() {
  const pauseOnIOThreadMS = 2 * 1000;

  const principal = PrincipalUtils.createPrincipal("https://example.com");
  const name = "test_slowStorageInitialization.js";

  info("Testing origin clearing requested after starting database work");

  info("Setting pref");

  Services.prefs.setIntPref(
    "dom.indexedDB.databaseInitialization.pauseOnIOThreadMs",
    pauseOnIOThreadMS
  );

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

  Services.prefs.clearUserPref(
    "dom.indexedDB.databaseInitialization.pauseOnIOThreadMs"
  );
}
