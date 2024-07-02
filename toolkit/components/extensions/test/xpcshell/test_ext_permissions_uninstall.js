"use strict";

const { ExtensionPermissions } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

const { ExtensionUninstallTracker } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionTestCommon.sys.mjs"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

// This test doesn't need the test extensions to be detected as privileged,
// disabling it to avoid having to keep the list of expected "internal:*"
// permissions that are added automatically to privileged extensions
// and already covered by other tests.
AddonTestUtils.usePrivilegedSignatures = false;

// Look up the cached permissions, if any.
async function getCachedPermissions(extensionId) {
  const NotFound = Symbol("extension ID not found in permissions cache");
  try {
    return await ExtensionParent.StartupCache.permissions.get(
      extensionId,
      () => {
        // Throw error to prevent the key from being created.
        throw NotFound;
      }
    );
  } catch (e) {
    if (e === NotFound) {
      return null;
    }
    throw e;
  }
}

// Look up the permissions from the file. Internal methods are used to avoid
// inadvertently changing the permissions in the cache or the database.
async function getStoredPermissions(extensionId) {
  if (await ExtensionPermissions._has(extensionId)) {
    return ExtensionPermissions._get(extensionId);
  }
  return null;
}

add_setup(async function setup() {
  // Bug 1646182: Force ExtensionPermissions to run in rkv mode, the legacy
  // storage mode will run in xpcshell-legacy-ep.toml
  await ExtensionPermissions._uninit();

  optionalPermissionsPromptHandler.init();
  optionalPermissionsPromptHandler.acceptPrompt = true;

  await AddonTestUtils.promiseStartupManager();
  registerCleanupFunction(async () => {
    await AddonTestUtils.promiseShutdownManager();
  });
});

// This test must run before any restart of the addonmanager so the
// ExtensionAddonObserver works.
add_task(async function test_permissions_removed() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      optional_permissions: ["idle"],
    },
    background() {
      browser.test.onMessage.addListener(async (msg, arg) => {
        if (msg == "request") {
          try {
            let result = await browser.permissions.request(arg);
            browser.test.sendMessage("request.result", result);
          } catch (err) {
            browser.test.sendMessage("request.result", err.message);
          }
        }
      });
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request", { permissions: ["idle"], origins: [] });
    let result = await extension.awaitMessage("request.result");
    equal(result, true, "request() for optional permissions succeeded");
  });

  let id = extension.id;
  let perms = await ExtensionPermissions.get(id);
  equal(
    perms.permissions.length,
    1,
    `optional permission added (${JSON.stringify(perms.permissions)})`
  );

  Assert.deepEqual(
    await getCachedPermissions(id),
    {
      permissions: ["idle"],
      origins: [],
    },
    "Optional permission added to cache"
  );
  Assert.deepEqual(
    await getStoredPermissions(id),
    {
      permissions: ["idle"],
      origins: [],
    },
    "Optional permission added to persistent file"
  );

  await extension.unload();

  // Directly read from the internals instead of using ExtensionPermissions.get,
  // because the latter will lazily cache the extension ID.
  Assert.deepEqual(
    await getCachedPermissions(id),
    null,
    "Cached permissions removed"
  );
  Assert.deepEqual(
    await getStoredPermissions(id),
    null,
    "Stored permissions removed"
  );

  perms = await ExtensionPermissions.get(id);
  equal(
    perms.permissions.length,
    0,
    `no permissions after uninstall (${JSON.stringify(perms.permissions)})`
  );
  equal(
    perms.origins.length,
    0,
    `no origin permissions after uninstall (${JSON.stringify(perms.origins)})`
  );

  // The public ExtensionPermissions.get method should not store (empty)
  // permissions in the persistent database. Polluting the cache is not ideal,
  // but acceptable since the cache will eventually be cleared, and non-test
  // code is not likely to call ExtensionPermissions.get() for non-installed
  // extensions anyway.
  Assert.deepEqual(await getCachedPermissions(id), perms, "Permissions cached");
  Assert.deepEqual(
    await getStoredPermissions(id),
    null,
    "Permissions not saved"
  );
});

// Regression test for bug 1902011: Verifies that a slow write does not result
// in the extension permissions continuing to be around post extension unload.
add_task(async function test_simulate_slow_storage() {
  await ExtensionPermissions._uninit();
  const interceptedStorePutCalls = [];
  const deferredPut = Promise.withResolvers();
  let store = ExtensionPermissions._getStore();
  let originalPut = store.put;
  store.put = async function (extensionId, permissions) {
    // _setupStartupPermissions in Extension.sys.mjs will call us indirectly as
    // part as granting host_permissions of the MV3 test extension.
    interceptedStorePutCalls.push({ extensionId, permissions });
    info("Intercepted and suspended store.put call");
    await deferredPut.promise;
    info("Continuing store.put call");
    return originalPut.apply(this, arguments);
  };

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      host_permissions: ["*://slow.example.com/*"],
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  const id = extension.id;

  // We are simulating slow writes, but don't delay reads. As part of loading
  // an extension for the first time there is an ExtensionPermissions.get call
  // that initializes the cached permissions (before host_permissions are
  // granted via _setupStartupPermissions in Extension.sys.mjs).
  Assert.deepEqual(
    await getCachedPermissions(id),
    { permissions: [], origins: [] },
    "Cached permissions should be present despite simulated slow storage"
  );
  Assert.deepEqual(
    await getStoredPermissions(id),
    null,
    "No stored permissions due to the simulated slow storage"
  );

  let cleanupDone = false;
  let removeAllDone = false;
  Management.once("cleanupAfterUninstall", (_, addonId, tasks) => {
    equal(addonId, id, "Got cleanupAfterUninstall for expected extension");
    ok(!cleanupDone, "Extension cleanup has not finished yet");
    const name = `Clear ExtensionPermissions for ${addonId}`;
    let removeAllPromise = tasks.find(t => t.name === name).promise;
    removeAllPromise.then(() => {
      removeAllDone = true;
    });
    // Delay for a bit to allow any other async tasks to potentially complete
    // and potentially pollute any state.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(() => {
      ok(!removeAllDone, "ExtensionPermissions.removeAll() is still pending");
      ok(!cleanupDone, "Extension cleanup has still not finished yet");
      deferredPut.resolve();
    }, 1000);
  });

  const uninstallTracker = new ExtensionUninstallTracker(id);
  await extension.unload();

  let unrelatedExt = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 2,
      permissions: ["tabs", "*://example.com/*"],
      optional_permissions: ["webNavigation"],
      browser_specific_settings: { gecko: { id: "unrelated@ext-id" } },
    },
    useAddonManager: "temporary",
  });
  // As explained earlier, ExtensionPermissions.get is called for the first
  // startup. unrelatedExt's startup should not be affected by the write
  // operation of |extension|.
  info("Unrelated extension can start up without delay");
  await unrelatedExt.startup();

  // Note: waitForUninstallCleanupDone() depends on removeAll()'s completion,
  // which should not resolve until we call deferredPut.resolve().
  ok(!removeAllDone, "ExtensionPermissions.removeAll() not done yet");
  await uninstallTracker.waitForUninstallCleanupDone();
  cleanupDone = true;
  ok(removeAllDone, "ExtensionPermissions.removeAll() completed");
  Assert.deepEqual(
    await getCachedPermissions(id),
    null,
    "No cached permissions past extension uninstall"
  );
  Assert.deepEqual(
    await getStoredPermissions(id),
    null,
    "No stored permissions past extension uninstall"
  );

  store.put = originalPut;
  Assert.deepEqual(
    interceptedStorePutCalls,
    [
      {
        extensionId: id,
        permissions: {
          permissions: [],
          origins: ["*://slow.example.com/*"],
        },
      },
    ],
    "Observed attempt to write host_permissions through ExtensionPermissions"
  );

  // Now verify that the unrelated extension can unload as usual now that we
  // have the usual non-delayed writes.
  const uninstallTracker2 = new ExtensionUninstallTracker(unrelatedExt.id);
  await unrelatedExt.unload();
  info("Unrelated extension unloaded, waiting for cleanup");
  await uninstallTracker2.waitForUninstallCleanupDone();
  info("Unrelated extension managed to uninstall and cleanup");
});
