/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test doesn't use the shared module system (running the same test in
// multiple test suites) on purpose because it needs to create an unprivileged
// sandbox which is not possible if the test is already running in a sandbox.

const { PrincipalUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/PrincipalUtils.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

add_task(async function testSteps() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");

  info("Starting database opening");

  const openPromise = new Promise(function (resolve, reject) {
    const sandbox = new Cu.Sandbox(principal, {
      wantGlobalProperties: ["storage"],
      forceSecureContext: true,
    });
    sandbox.resolve = resolve;
    sandbox.reject = reject;
    Cu.evalInSandbox(`storage.getDirectory().then(resolve, reject);`, sandbox);
  });

  info("Waiting for database work to start");

  await TestUtils.topicObserved("BucketFS::DatabaseWorkStarted");

  info("Waiting for database to finish opening");

  await openPromise;
});
