/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Enable SCOPE_APPLICATION for builtin testing.  Default in tests is only SCOPE_PROFILE.
let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
Services.prefs.setIntPref("extensions.enabledScopes", scopes);

// This verifies that system addon about:config prefs
// are honored during startup/restarts/upgrades.
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2");

let distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "empty"]);
distroDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
registerDirectory("XREAppFeat", distroDir);

AddonTestUtils.usePrivilegedSignatures = "system";

BootstrapMonitor.init();

add_setup(async function setup() {
  await initSystemAddonDirs();
  let xpi = await getSystemAddonXPI(1, "1.0");
  await AddonTestUtils.manuallyInstall(xpi, distroDir);
});

async function systemAddonPreffedOff({ asBuiltIn = true } = {}) {
  const id = "system1@tests.mozilla.org";
  Services.prefs.setBoolPref("extensions.system1.enabled", false);

  await overrideBuiltIns({
    builtins: asBuiltIn ? [await getSystemBuiltin(1, "1.0")] : [],
    system: !asBuiltIn ? [id] : [],
  });

  await promiseStartupManager("1.0");

  BootstrapMonitor.checkInstalled(id);
  BootstrapMonitor.checkNotStarted(id);

  await promiseRestartManager();

  BootstrapMonitor.checkNotStarted(id);

  await promiseShutdownManager({ clearOverrides: false });
}

async function systemAddonPreffedOn({ asBuiltIn = true } = {}) {
  const id = "system1@tests.mozilla.org";
  Services.prefs.setBoolPref("extensions.system1.enabled", true);

  await overrideBuiltIns({
    builtins: asBuiltIn ? [await getSystemBuiltin(1, "1.0")] : [],
    system: !asBuiltIn ? [id] : [],
  });

  await promiseStartupManager("2.0");

  BootstrapMonitor.checkInstalled(id);
  BootstrapMonitor.checkStarted(id);

  await promiseRestartManager();

  BootstrapMonitor.checkStarted(id);

  await promiseShutdownManager({ clearOverrides: false });
}

// TODO(Bug 1949847): remove this test along with removing the app-system-defaults location.
add_task(async function systemPref_xpi() {
  await systemAddonPreffedOff({ asBuiltIn: false });
  await systemAddonPreffedOn({ asBuiltIn: false });
});

add_task(async function systemPref_builtin() {
  await systemAddonPreffedOff();
  await systemAddonPreffedOn();
});
