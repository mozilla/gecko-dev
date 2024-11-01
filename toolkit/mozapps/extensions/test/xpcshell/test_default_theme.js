"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

// AddonTestUtils.init restricts enabledScopes to SCOPE_PROFILE only. Here we
// want to test builtin themes, which requires SCOPE_APPLICATION to be set.
// This matches the behavior in production.
Services.prefs.setIntPref(
  "extensions.enabledScopes",
  AddonManager.SCOPE_APPLICATION
);
ExtensionTestUtils.mockAppInfo();

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

function getXPIProviderSpies() {
  const sandbox = sinon.createSandbox();
  const { XPIProvider } = AddonTestUtils.getXPIExports();
  return {
    sandbox,
    maybeInstallBuiltinAddon: sandbox.spy(
      XPIProvider,
      "maybeInstallBuiltinAddon"
    ),
    installBuiltinAddon: sandbox.spy(XPIProvider, "installBuiltinAddon"),
  };
}

// XPIProvider.sys.mjs contains a maybeInstallBuiltinAddon() call that has a
// hardcoded add-on ID and version ID. This should match the actual version of
// the add-on at toolkit/mozapps/extensions/default-theme/manifest.json
// (exposed at resource://default-theme/manifest.json).
// If they are inconsistent, then it will be re-installed at every startup, as
// seen in: https://bugzilla.mozilla.org/show_bug.cgi?id=1928082
add_task(async function installs_default_theme_at_addonmanager_startup() {
  const spies = getXPIProviderSpies();

  await promiseStartupManager();

  let defaultThemeAddon = await AddonManager.getAddonByID(DEFAULT_THEME_ID);

  info("maybeInstallBuiltinAddon() should be called with the right version");
  sinon.assert.calledOnce(spies.maybeInstallBuiltinAddon);
  sinon.assert.calledWithExactly(
    spies.maybeInstallBuiltinAddon,
    DEFAULT_THEME_ID,
    defaultThemeAddon.version,
    "resource://default-theme/"
  );

  info("maybeInstallBuiltinAddon() should trigger actual installation");
  sinon.assert.calledOnce(spies.installBuiltinAddon);
  sinon.assert.calledWithExactly(
    spies.installBuiltinAddon,
    "resource://default-theme/"
  );

  spies.sandbox.restore();

  await promiseShutdownManager();
});

// While the other test confirms that we are calling maybeInstallBuiltinAddon
// with the right parameters, here we confirm that the default theme is not
// repeatedly re-installed on startup, which is the user-observable consequence
// if the version were to be incorrect, as seen at:
// https://bugzilla.mozilla.org/show_bug.cgi?id=1928082
add_task(async function does_not_reinstall_default_theme_after_restart() {
  const spies = getXPIProviderSpies();

  await promiseStartupManager();

  // maybeInstallBuiltinAddon still called, like the other test task.
  sinon.assert.calledOnce(spies.maybeInstallBuiltinAddon);
  sinon.assert.calledWith(spies.maybeInstallBuiltinAddon, DEFAULT_THEME_ID);

  info("maybeInstallBuiltinAddon() should not trigger re-installation");
  sinon.assert.notCalled(spies.installBuiltinAddon);

  spies.sandbox.restore();

  await promiseShutdownManager();
});
