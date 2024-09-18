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
const { QuotaUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/QuotaUtils.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

add_task(
  {
    pref_set: [
      ["dom.quotaManager.storageInitialization.pauseOnIOThreadMs", 2000],
    ],
  },
  async function testSteps() {
    const principal = PrincipalUtils.createPrincipal("https://example.com");

    info(
      "Testing origin clearing requested after starting client directory opening"
    );

    info("Starting database opening");

    const openPromise = new Promise(function (resolve, reject) {
      const sandbox = new Cu.Sandbox(principal, {
        wantGlobalProperties: ["storage"],
        forceSecureContext: true,
      });
      sandbox.resolve = resolve;
      sandbox.reject = reject;
      Cu.evalInSandbox(
        `storage.getDirectory().then(resolve, reject);`,
        sandbox
      );
    });

    info("Waiting for client directory opening to start");

    await TestUtils.topicObserved(
      "QuotaManager::ClientDirectoryOpeningStarted"
    );

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
);
