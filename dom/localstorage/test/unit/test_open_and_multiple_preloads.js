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

/**
 * Stress test for concurrent storage opening and storage preloads with
 * simulated artificial failures.
 *
 * This test creates a principal for "https://example.com" and simulates a
 * high-load environment by initiating one storage opening in a child process
 * and 10 parallel storage preloads. During these operations, artificial
 * failures in `QuotaManager::OpenClientDirectory` are triggered with a 20%
 * probability, testing the system's serialization of prepare datastore
 * operations.
 */
add_task(async function testSteps() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");

  info("Clearing");

  {
    const request = Services.qms.clear();
    await QuotaUtils.requestFinished(request);
  }

  info("Installing package");

  installPackage("somedata_profile");

  await QuotaUtils.withArtificialFailures(
    Ci.nsIQuotaArtificialFailure.CATEGORY_OPEN_CLIENT_DIRECTORY,
    /* probability */ 20,
    Cr.NS_ERROR_FAILURE,
    async function () {
      const promises = [];

      promises.push(run_test_in_child("open_and_multiple_preloads_child.js"));

      await do_await_remote_message("LocalStorageTest::ChildReady");

      for (let i = 0; i < 10; i++) {
        promises.push(Services.domStorageManager.preload(principal));
      }

      try {
        await Promise.allSettled(promises);
      } catch (ex) {}
    }
  );
});
