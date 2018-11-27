/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that forcing undo for uninstall works

const APP_STARTUP                     = 1;
const APP_SHUTDOWN                    = 2;
const ADDON_DISABLE                   = 4;
const ADDON_INSTALL                   = 5;
const ADDON_UNINSTALL                 = 6;
const ADDON_UPGRADE                   = 7;

const ID = "undouninstall1@tests.mozilla.org";
const INCOMPAT_ID = "incompatible@tests.mozilla.org";


const profileDir = gProfD.clone();
profileDir.append("extensions");

const ADDONS = {
  test_undoincompatible: {
    manifest: {
      name: "Incompatible Addon",
      applications: {
        gecko: {
          id: "incompatible@tests.mozilla.org",
          strict_min_version: "2",
          strict_max_version: "2",
        },
      },
    },
  },
  test_undouninstall1: {
    manifest: {
      name: "Test Bootstrap 1",
      applications: {
        gecko: {
          id: "undouninstall1@tests.mozilla.org",
        },
      },
    },
  },
};

const XPIS = {};

let Monitor = SlightlyLessDodgyBootstrapMonitor;
Monitor.init();

function getStartupReason(id) {
  let info = Monitor.started.get(id);
  return info ? info.reason : undefined;
}

function getShutdownReason(id) {
  let info = Monitor.stopped.get(id);
  return info ? info.reason : undefined;
}

function getInstallReason(id) {
  let info = Monitor.installed.get(id);
  return info ? info.reason : undefined;
}

function getUninstallReason(id) {
  let info = Monitor.uninstalled.get(id);
  return info ? info.reason : undefined;
}

function getShutdownNewVersion(id) {
  let info = Monitor.stopped.get(id);
  return info ? info.params.newVersion : undefined;
}

// Sets up the profile by installing an add-on.
add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  await promiseStartupManager();
  registerCleanupFunction(promiseShutdownManager);

  for (let [name, files] of Object.entries(ADDONS)) {
    XPIS[name] = await AddonTestUtils.createTempWebExtensionFile(files);
  }
});

// Tests that an enabled restartless add-on can be uninstalled and goes away
// when the uninstall is committed
add_task(async function uninstallRestartless() {
  await promiseInstallFile(XPIS.test_undouninstall1);

  let a1 = await promiseAddonByID(ID);

  Assert.notEqual(a1, null);
  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  await a1.uninstall(true);

  a1 = await promiseAddonByID(ID);

  Assert.notEqual(a1, null);
  Monitor.checkInstalled(ID);
  Monitor.checkNotStarted(ID);
  Assert.equal(getShutdownReason(ID), ADDON_UNINSTALL);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);
  Assert.ok(!a1.userDisabled);

  await a1.uninstall();

  a1 = await promiseAddonByID(ID);

  Assert.equal(a1, null);
  Monitor.checkNotStarted(ID);
});

// Tests that an enabled restartless add-on can be uninstalled and then cancelled
add_task(async function cancelUninstallOfRestartless() {
  await promiseInstallFile(XPIS.test_undouninstall1);
  let a1 = await promiseAddonByID(ID);

  Assert.notEqual(a1, null);
  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling",
    ],
  });
  await a1.uninstall(true);
  ensure_test_completed();

  clearListeners();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkInstalled(ID);
  Monitor.checkNotStarted(ID);
  Assert.equal(getShutdownReason(ID), ADDON_UNINSTALL);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);
  Assert.ok(!a1.userDisabled);

  let promises = [
    promiseAddonEvent("onOperationCancelled"),
    promiseWebExtensionStartup(ID),
  ];
  a1.cancelUninstall();
  await Promise.all(promises);

  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkStarted(ID, "1.0");
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  await promiseShutdownManager();

  Assert.equal(getShutdownReason(ID), APP_SHUTDOWN);
  Assert.equal(getShutdownNewVersion(ID), undefined);

  await promiseStartupManager();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkStarted(ID, "1.0");
  Assert.equal(getStartupReason(ID), APP_STARTUP);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  await a1.uninstall();
});

// Tests that reinstalling an enabled restartless add-on waiting to be
// uninstalled aborts the uninstall and leaves the add-on enabled
add_task(async function reinstallAddonAwaitingUninstall() {
  await promiseInstallFile(XPIS.test_undouninstall1);

  let a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling",
    ],
  });
  await a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkInstalled(ID);
  Monitor.checkNotStarted(ID);
  Assert.equal(getShutdownReason(ID), ADDON_UNINSTALL);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);
  Assert.ok(!a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled",
    ],
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded",
  ]);

  await promiseInstallFile(XPIS.test_undouninstall1);

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  ensure_test_completed();

  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_UPGRADE);
  Assert.equal(getStartupReason(ID), ADDON_UPGRADE);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  await promiseShutdownManager();

  Assert.equal(getShutdownReason(ID), APP_SHUTDOWN);

  await promiseStartupManager();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkStarted(ID, "1.0");
  Assert.equal(getStartupReason(ID), APP_STARTUP);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  await a1.uninstall();
});

// Tests that a disabled restartless add-on can be uninstalled and goes away
// when the uninstall is committed
add_task(async function uninstallDisabledRestartless() {
  await promiseInstallFile(XPIS.test_undouninstall1);

  let a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  await a1.disable();
  Monitor.checkNotStarted(ID);
  Assert.equal(getShutdownReason(ID), ADDON_DISABLE);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling",
    ],
  });
  await a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkNotStarted(ID);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  // commit the uninstall
  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalled",
    ],
  });
  await a1.uninstall();
  ensure_test_completed();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.equal(a1, null);
  Monitor.checkNotStarted(ID);
  Monitor.checkNotInstalled(ID);
  Assert.equal(getUninstallReason(ID), ADDON_UNINSTALL);
});

// Tests that a disabled restartless add-on can be uninstalled and then cancelled
add_task(async function cancelUninstallDisabledRestartless() {
  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled",
    ],
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded",
  ]);
  await promiseInstallFile(XPIS.test_undouninstall1);
  ensure_test_completed();

  let a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      ["onDisabling", false],
      "onDisabled",
    ],
  });
  await a1.disable();
  ensure_test_completed();

  Monitor.checkNotStarted(ID);
  Assert.equal(getShutdownReason(ID), ADDON_DISABLE);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling",
    ],
  });
  await a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkNotStarted(ID);
  Monitor.checkInstalled(ID);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onOperationCancelled",
    ],
  });
  a1.cancelUninstall();
  ensure_test_completed();

  Monitor.checkNotStarted(ID);
  Monitor.checkInstalled(ID);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  await promiseRestartManager();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkNotStarted(ID);
  Monitor.checkInstalled(ID);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  await a1.uninstall();
});

// Tests that reinstalling a disabled restartless add-on waiting to be
// uninstalled aborts the uninstall and leaves the add-on disabled
add_task(async function reinstallDisabledAddonAwaitingUninstall() {
  await promiseInstallFile(XPIS.test_undouninstall1);

  let a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  await a1.disable();
  Monitor.checkNotStarted(ID);
  Assert.equal(getShutdownReason(ID), ADDON_DISABLE);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling",
    ],
  });
  await a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkNotStarted(ID);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      ["onInstalling", false],
      "onInstalled",
    ],
  }, [
    "onNewInstall",
    "onInstallStarted",
    "onInstallEnded",
  ]);

  await promiseInstallFile(XPIS.test_undouninstall1);

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  ensure_test_completed();

  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkNotStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_UPGRADE);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  await promiseRestartManager();

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkNotStarted(ID, "1.0");
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(!a1.isActive);
  Assert.ok(a1.userDisabled);

  await a1.uninstall();
});


// Test that uninstalling a temporary addon can be canceled
add_task(async function cancelUninstallTemporary() {
  await AddonManager.installTemporaryAddon(XPIS.test_undouninstall1);

  let a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");
  Assert.notEqual(a1, null);
  Monitor.checkInstalled(ID, "1.0");
  Monitor.checkStarted(ID, "1.0");
  Assert.equal(getInstallReason(ID), ADDON_INSTALL);
  Assert.equal(getStartupReason(ID), ADDON_INSTALL);
  Assert.equal(a1.pendingOperations, AddonManager.PENDING_NONE);
  Assert.ok(a1.isActive);
  Assert.ok(!a1.userDisabled);

  prepare_test({
    "undouninstall1@tests.mozilla.org": [
      "onUninstalling",
    ],
  });
  await a1.uninstall(true);
  ensure_test_completed();

  clearListeners();

  Monitor.checkNotStarted(ID, "1.0");
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));

  let promises = [
    promiseAddonEvent("onOperationCancelled"),
    promiseWebExtensionStartup("undouninstall1@tests.mozilla.org"),
  ];
  a1.cancelUninstall();
  await Promise.all(promises);

  a1 = await promiseAddonByID("undouninstall1@tests.mozilla.org");

  Assert.notEqual(a1, null);
  Monitor.checkStarted(ID, "1.0");
  Assert.equal(a1.pendingOperations, 0);

  await promiseRestartManager();
});

// Tests that cancelling the uninstall of an incompatible restartless addon
// does not start the addon
add_task(async function cancelUninstallIncompatibleRestartless() {
  await promiseInstallFile(XPIS.test_undoincompatible);

  let a1 = await promiseAddonByID(INCOMPAT_ID);
  Assert.notEqual(a1, null);
  Monitor.checkNotStarted(INCOMPAT_ID);
  Assert.ok(!a1.isActive);

  prepare_test({
    "incompatible@tests.mozilla.org": [
      "onUninstalling",
    ],
  });
  await a1.uninstall(true);
  ensure_test_completed();

  a1 = await promiseAddonByID(INCOMPAT_ID);
  Assert.notEqual(a1, null);
  Assert.ok(hasFlag(AddonManager.PENDING_UNINSTALL, a1.pendingOperations));
  Assert.ok(!a1.isActive);

  prepare_test({
    "incompatible@tests.mozilla.org": [
      "onOperationCancelled",
    ],
  });
  a1.cancelUninstall();
  ensure_test_completed();

  a1 = await promiseAddonByID(INCOMPAT_ID);
  Assert.notEqual(a1, null);
  Monitor.checkNotStarted(INCOMPAT_ID);
  Assert.equal(a1.pendingOperations, 0);
  Assert.ok(!a1.isActive);
});
