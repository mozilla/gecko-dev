/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { PrincipalUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/PrincipalUtils.sys.mjs"
);
const { IndexedDBUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/indexedDB/test/modules/IndexedDBUtils.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

/* exported testSteps */
async function testSteps() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");
  const name = "test_databaseWorkStarted.js";

  info("Starting database opening");

  const openPromise = (async function () {
    const request = indexedDB.openForPrincipal(principal, name);
    const promise = IndexedDBUtils.requestFinished(request);
    return promise;
  })();

  info("Waiting for database work to start");

  await TestUtils.topicObserved("IndexedDB::DatabaseWorkStarted");

  info("Waiting for database to finish opening");

  await openPromise;
}
