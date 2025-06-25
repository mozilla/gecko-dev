/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { FileUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/FileUtils.sys.mjs"
);
const { PrincipalUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/PrincipalUtils.sys.mjs"
);
const { QuotaUtils } = ChromeUtils.importESModule(
  "resource://testing-common/dom/quota/test/modules/QuotaUtils.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

/**
 * This test verifies the behavior when a shutdown occurs during the
 * initialization of all temporary origins in the background.
 *
 * Test Setup & Procedure:
 * 1. Lazy Origin Initialization Enabled
 *    - The test runs with lazy origin initialization enabled, meaning that
 *      calling `Services.qms.initTemporaryStorage` does not initialize
 *      individual origins.
 *
 * 2. Preparation of Existing Profile
 *    - Two origins belonging to the same eTLD+1 group are created.
 *
 * 3. Removal of Metadata Files
 *    - Metadata files are removed to force restoration, providing a way
 *      to check whether origin initialization has occurred.
 *
 * 4. Triggering Initialization of All Temporary Origins
 *    - The test explicitly triggers the initialization of all temporary
 *      origins by calling `Services.qms.initializeAllTemporaryOrigins`.
 *
 * 5. Inducing a Controlled Shutdown
 *    - The test waits for the first origin to begin initialization.
 *    - Once initialization starts, shutdown is triggered.
 *    - A 2-second delay is introduced in origin initialization to ensure
 *      the shutdown signal has time to propagate.
 *
 * 6. Expected Behavior & Verification
 *    - Due to the shutdown, the second origin does not initialize.
 *    - Since active operations are not available during shutdown, the test
 *      verifies initialization by checking whether the metadata file has
 *      been restored for the given origin.
 *
 * Notes & Justification:
 * - This approach has been successfully used in other tests to handle
 *   shutdown scenarios.
 * - See also:
 *   https://searchfox.org/mozilla-central/rev/d5baa11e35e0186c3c867f4948010f0742198467/modules/libpref/init/StaticPrefList.yaml#3789-3846
 */
async function testShutdownDuringAllTemporaryOriginsInitialization() {
  // This test creates two origins belonging to the same eTLD+1 group on
  // purpose to test specific shutdown check.
  const principal1 = PrincipalUtils.createPrincipal("https://1.example.com");
  const principal2 = PrincipalUtils.createPrincipal("https://2.example.com");
  const metadata1 = FileUtils.getFile(
    "storage/default/https+++1.example.com/.metadata-v2"
  );
  const metadata2 = FileUtils.getFile(
    "storage/default/https+++2.example.com/.metadata-v2"
  );

  // Preparation phase: simulating an existing profile with pre-existing
  // storage. At least two temporary origins must already exist.
  info("Clearing storage");

  {
    const request = Services.qms.clear();
    await QuotaUtils.requestFinished(request);
  }

  info("Initializing storage");

  {
    const request = Services.qms.init();
    await QuotaUtils.requestFinished(request);
  }

  info("Initializing temporary storage");

  {
    const request = Services.qms.initTemporaryStorage();
    await QuotaUtils.requestFinished(request);
  }

  info("Initializing temporary origin");

  {
    const request = Services.qms.initializeTemporaryOrigin(
      "default",
      principal1,
      /* aCreateIfNonExistent */ true
    );
    await QuotaUtils.requestFinished(request);
  }

  info("Initializing temporary origin");

  {
    const request = Services.qms.initializeTemporaryOrigin(
      "default",
      principal2,
      /* aCreateIfNonExistent */ true
    );
    await QuotaUtils.requestFinished(request);
  }

  // Restore the original uninitialized state to force reinitialization of both
  // storage and temporary storage.
  info("Shutting down storage");

  {
    const request = Services.qms.reset();
    await QuotaUtils.requestFinished(request);
  }

  // Reinitialize storage and temporary storage.
  info("Initializing storage");

  {
    const request = Services.qms.init();
    await QuotaUtils.requestFinished(request);
  }

  info("Initializing temporary storage");

  {
    const request = Services.qms.initTemporaryStorage();
    await QuotaUtils.requestFinished(request);
  }

  info("Removing origin metadata");

  metadata1.remove(false);

  info("Removing origin metadata");

  metadata2.remove(false);

  // Now trigger the initialization of all temporary origins, which normally
  // happens automatically when incremental origin initialization is enabled
  // and temporary storage initialization is complete.
  info("Starting all temporary origins initialization");

  const initPromise = (async function () {
    const request = Services.qms.initializeAllTemporaryOrigins();
    const promise = QuotaUtils.requestFinished(request);
    return promise;
  })();

  info("Waiting for origin initialization to start");

  await TestUtils.topicObserved("QuotaManager::OriginInitializationStarted");

  info("Starting shutdown");

  QuotaUtils.startShutdown();

  info("Waiting for all temoporary origins initialization to finish");

  await initPromise;

  info("Metadata file for first origin exists: " + metadata1.exists());

  info("Metadata file for second origin exists: " + metadata2.exists());

  // Once shutdown has started, we can no longer use the Quota Manager Service
  // to check the initialization status of origins, as any new operation will
  // fail. However, we can determine if the initialization of all origins was
  // aborted due to shutdown by checking for the existence of origin metadata
  // files, which were artificially removed earlier.
  Assert.notEqual(
    metadata1.exists(),
    metadata2.exists(),
    "Metadata for the second origin is not restored"
  );
}

/* exported testSteps */
async function testSteps() {
  add_task(
    {
      pref_set: [
        ["dom.quotaManager.temporaryStorage.lazyOriginInitialization", true],
        ["dom.quotaManager.loadQuotaFromCache", false],
        ["dom.quotaManager.originInitialization.pauseOnIOThreadMs", 2000],
      ],
    },
    testShutdownDuringAllTemporaryOriginsInitialization
  );
}
