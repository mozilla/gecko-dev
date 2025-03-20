// Enable signature checks for these tests
gUseRealCertChecks = true;

const DATA = "data/signing_checks";
const ID = "test@somewhere.com";

const profileDir = gProfD.clone();
profileDir.append("extensions");

function verifySignatures() {
  return new Promise(resolve => {
    let observer = (subject, topic, data) => {
      Services.obs.removeObserver(observer, "xpi-signature-changed");
      resolve(JSON.parse(data));
    };
    Services.obs.addObserver(observer, "xpi-signature-changed");

    info("Verifying signatures");
    const { XPIDatabase } = ChromeUtils.import(
      "resource://gre/modules/addons/XPIDatabase.jsm"
    );
    XPIDatabase.verifySignatures();
  });
}

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "4", "48");

add_task(async function test_no_change() {
  await promiseStartupManager();

  // Install the first add-on
  await promiseInstallFile(do_get_file(`${DATA}/signed1.xpi`));

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.appDisabled, false);
  Assert.equal(addon.isActive, true);
  Assert.equal(addon.pendingOperations, AddonManager.PENDING_NONE);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);

  // Swap in the files from the next add-on
  manuallyUninstall(profileDir, ID);
  await manuallyInstall(do_get_file(`${DATA}/signed2.xpi`), profileDir, ID);

  let listener = {
    onPropetyChanged(_addon, properties) {
      Assert.ok(false, `Got unexpected onPropertyChanged for ${_addon.id}`);
    },
  };

  AddonManager.addAddonListener(listener);

  // Trigger the check
  let changes = await verifySignatures();
  Assert.equal(changes.enabled.length, 0);
  Assert.equal(changes.disabled.length, 0);

  Assert.equal(addon.appDisabled, false);
  Assert.equal(addon.isActive, true);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);

  await addon.uninstall();
  AddonManager.removeAddonListener(listener);
});

add_task(async function test_diable() {
  // Install the first add-on
  await promiseInstallFile(do_get_file(`${DATA}/signed1.xpi`));

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.ok(addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);

  // Swap in the files from the next add-on
  manuallyUninstall(profileDir, ID);
  await manuallyInstall(do_get_file(`${DATA}/unsigned.xpi`), profileDir, ID);

  let changedProperties = [];
  let listener = {
    onPropertyChanged(_, properties) {
      changedProperties.push(...properties);
    },
  };
  AddonManager.addAddonListener(listener);

  // Trigger the check
  let [changes] = await Promise.all([
    verifySignatures(),
    promiseAddonEvent("onDisabling"),
  ]);

  Assert.equal(changes.enabled.length, 0);
  Assert.equal(changes.disabled.length, 1);
  Assert.equal(changes.disabled[0], ID);

  Assert.deepEqual(
    changedProperties,
    ["signedState", "appDisabled"],
    "Got onPropertyChanged events for signedState and appDisabled"
  );

  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);

  await addon.uninstall();
  AddonManager.removeAddonListener(listener);
});

// Regression test for https://bugzilla.mozilla.org/show_bug.cgi?id=1954818
//
// Do NOT remove this test or the XPI files. If this test becomes obsolete due
// to dropped support for these XPI files (e.g. if support for add-ons with
// SHA-1 signatures were to be dropped entirely), don't forget to delete
// addons-public-2018-intermediate.pem (undo the patch to bug 1954818).
add_task(async function test_xpi_signed_in_or_before_feb_2018() {
  // Disable schema warnings for two reasons:
  // - The "commands" property in the manifest is not supported on Android.
  // - The resigned version "2resigned1" results in the following warning:
  //   "version must be a version string consisting of at most 4 integers of at
  //   most 9 digits without leading zeros, and separated with dots"
  ExtensionTestUtils.failOnSchemaWarnings(false);

  async function checkAddonIsValid(xpiPath) {
    let { addon } = await promiseInstallFile(do_get_file(xpiPath));
    Assert.notEqual(addon, null);
    Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);
    Assert.ok(addon.isActive);
    Assert.equal(addon.appDisabled, false);
    await addon.uninstall();
  }

  // The test extension is chosen such that it was signed before 2018, because
  // that was signed with CN=production-signing-ca.addons.mozilla.org
  // instead of CN=signingca1.addons.mozilla.org (used after 8 feb 2018).
  //
  // "disable-ctrl-q-and-cmd-q@robwu.nl" is a simple extension consisting of
  // one manifest.json. It was signed in 2016, and later resigned in 2024
  // because of enforcing stronger signatures (starting with bug 1885004).

  info("Checking add-on signed before 2018, 2016-12-22");
  await checkAddonIsValid(`${DATA}/disable_ctrl_q_and_cmd_q-1.xpi`);

  info("Checking add-on signed after 2018, 2024-04-25");
  await checkAddonIsValid(`${DATA}/disable_ctrl_q_and_cmd_q-2resigned1.xpi`);

  ExtensionTestUtils.failOnSchemaWarnings(true);
});
