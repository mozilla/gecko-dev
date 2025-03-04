/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const { ExtensionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionXPCShellUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.sys.mjs",
  ExtensionParent: "resource://gre/modules/ExtensionParent.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
});

do_get_profile();

// The following initializations are necessary in order to install the newtab
// built-in addon, if we're configured to do so.
ExtensionTestUtils.init(this);
AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

/**
 * Finds the built-in newtab addon code in the runtime environment directory
 * and then installs it.
 *
 * @returns {Promise<undefined>}
 *   Resolves once the addon has been installed.
 */
async function loadExtension() {
  let extensionPath = Services.dirsvc.get("GreD", Ci.nsIFile);
  extensionPath.append("browser");
  extensionPath.append("chrome");
  extensionPath.append("browser");
  extensionPath.append("builtin-addons");
  extensionPath.append("newtab");

  let startupPromise = new Promise(resolve => {
    const { apiManager } = ExtensionParent;
    function onReady(event, extension) {
      if (extension.id === "newtab@mozilla.org") {
        apiManager.off("ready", onReady);
        resolve();
      }
    }

    apiManager.on("ready", onReady);
  });

  await AddonManager.installTemporaryAddon(extensionPath);
  await startupPromise;
}

add_setup(async function head_initialize() {
  AddonTestUtils.createAppInfo(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "138"
  );
  await AddonTestUtils.promiseStartupManager();

  if (AppConstants.BROWSER_NEWTAB_AS_ADDON) {
    Services.prefs.setBoolPref("extensions.experiments.enabled", true);
    await loadExtension();
  }
});
