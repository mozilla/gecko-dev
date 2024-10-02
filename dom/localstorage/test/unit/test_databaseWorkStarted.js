/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { PrincipalUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/PrincipalUtils.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

add_task(async function testSteps() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");

  info("Starting database opening");

  const openPromise = Services.domStorageManager.preload(principal);

  info("Waiting for database work to start");

  await TestUtils.topicObserved("LocalStorage::DatabaseWorkStarted");

  info("Waiting for database to finish opening");

  await openPromise;
});
