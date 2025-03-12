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

async function testInitializeAllTemporaryOrigins() {
  const principal = PrincipalUtils.createPrincipal("https://example.com");

  // Preparation phase: simulating an existing profile with pre-existing
  // storage. At least one temporary origin must already exist.
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
      principal,
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

  // Temporary storage initialization shouldn't initialize origins when lazy
  // origin initialization is enabled.
  info("Verifying temporary origin");

  {
    const request = Services.qms.temporaryOriginInitialized(
      "default",
      principal
    );
    const initialized = await QuotaUtils.requestFinished(request);
    Assert.ok(!initialized, "Temporary origin is not initialized");
  }

  // Now trigger the initialization of all temporary origins, which normally
  // happens automatically when incremental origin initialization is enabled
  // and temporary storage initialization is complete.
  info("Initializing all temporary origins");

  {
    const request = Services.qms.initializeAllTemporaryOrigins();
    await QuotaUtils.requestFinished(request);
  }

  // The temporary origin should be now initialized.
  info("Verifying temporary origin");

  {
    const request = Services.qms.temporaryOriginInitialized(
      "default",
      principal
    );
    const initialized = await QuotaUtils.requestFinished(request);
    Assert.ok(initialized, "Temporary origin is initialized");
  }
}

/* exported testSteps */
async function testSteps() {
  add_task(
    {
      pref_set: [
        ["dom.quotaManager.temporaryStorage.lazyOriginInitialization", true],
        ["dom.quotaManager.loadQuotaFromCache", false],
      ],
    },
    testInitializeAllTemporaryOrigins
  );
}
