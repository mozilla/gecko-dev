/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { PrincipalUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/PrincipalUtils.sys.mjs"
);
const { SimpleDBUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/simpledb/test/modules/SimpleDBUtils.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

/* exported testSteps */
async function testSteps() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");
  const name = "test_clientDirectoryOpeningStarted.js";

  info("Starting database opening");

  const openPromise = (async function () {
    const connection = SimpleDBUtils.createConnection(principal);
    const request = connection.open(name);
    const promise = SimpleDBUtils.requestFinished(request);
    return promise;
  })();

  info("Waiting for client directory opening to start");

  await TestUtils.topicObserved("QuotaManager::ClientDirectoryOpeningStarted");

  info("Waiting for database to finish opening");

  await openPromise;
}
