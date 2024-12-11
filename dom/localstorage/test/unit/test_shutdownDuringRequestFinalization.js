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
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

add_task(
  {
    pref_set: [
      ["dom.storage.requestFinalization.pauseOnDOMFileThreadMs", 2000],
    ],
  },
  async function testSteps() {
    const principal = PrincipalUtils.createPrincipal("https://example.com");

    info("Testing shutdown requested after starting request finalization");

    // We need some existing data on disk, otherwise the preloading won't create
    // a datastore in memory that holds a directory lock.

    info("Clearing");

    {
      const request = Services.qms.clear();
      await QuotaUtils.requestFinished(request);
    }

    info("Installing package");

    installPackage("somedata_profile");

    info("Preloading");

    await Services.domStorageManager.preload(principal);

    info("Starting database opening");

    const openPromise = Services.domStorageManager.preload(principal);

    info("Waiting for request finalization to start");

    await TestUtils.topicObserved("LocalStorage::RequestFinalizationStarted");

    info("Starting shutdown");

    // XXX Extract this into a generic helper.
    const phases = [
      Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNNETTEARDOWN,
      Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNTEARDOWN,
      Services.startup.SHUTDOWN_PHASE_APPSHUTDOWN,
      Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNQM,
    ];

    for (const phase of phases) {
      Services.startup.advanceShutdownPhase(phase);
    }

    info("Waiting for database to finish opening");

    try {
      await openPromise;
      ok(false, "Should have thrown");
    } catch (e) {
      ok(true, "Should have thrown");
      Assert.strictEqual(
        e.result,
        Cr.NS_ERROR_ABORT,
        "Threw right result code"
      );
    }
  }
);
