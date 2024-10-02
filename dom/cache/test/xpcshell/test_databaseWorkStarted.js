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
  const name = "test_databaseWorkStarted.js";

  info("Starting database opening");

  const openPromise = new Promise(function (resolve, reject) {
    const sandbox = new Cu.Sandbox(principal, {
      wantGlobalProperties: ["caches"],
    });
    sandbox.resolve = resolve;
    sandbox.reject = reject;
    Cu.evalInSandbox(`caches.open("${name}").then(resolve, reject);`, sandbox);
  });

  info("Waiting for database work to start");

  await TestUtils.topicObserved("CacheAPI::DatabaseWorkStarted");

  info("Waiting for database to finish opening");

  await openPromise;
});
