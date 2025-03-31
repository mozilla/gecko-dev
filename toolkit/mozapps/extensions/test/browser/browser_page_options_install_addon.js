/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests bug 567127 - Add install button to the add-ons manager

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

var MockFilePicker = SpecialPowers.MockFilePicker;

AddonTestUtils.initMochitest(this);

const loadExtensionView = async () => {
  let win = await loadInitialView("extension");
  let doc = win.document;

  let pageOptionsMenu = doc.querySelector("addon-page-options panel-list");

  function openPageOptions() {
    let opened = BrowserTestUtils.waitForEvent(pageOptionsMenu, "shown");
    pageOptionsMenu.open = true;
    return opened;
  }

  function closePageOptions() {
    let closed = BrowserTestUtils.waitForEvent(pageOptionsMenu, "hidden");
    pageOptionsMenu.open = false;
    return closed;
  }

  return [win, openPageOptions, closePageOptions];
};

async function checkInstallConfirmation(
  names,
  { confirmInstall = false, expectUpdate = false } = {}
) {
  let notificationCount = 0;
  let observer = {
    observe(aSubject) {
      var installInfo = aSubject.wrappedJSObject;
      isnot(
        installInfo.browser,
        null,
        "Notification should have non-null browser"
      );
      Assert.deepEqual(
        installInfo.installs[0].installTelemetryInfo,
        {
          source: "about:addons",
          method: "install-from-file",
        },
        "Got the expected installTelemetryInfo"
      );
      notificationCount++;
    },
  };
  Services.obs.addObserver(observer, "addon-install-started");

  // The update doesn't have permissions, so it's automatically applied. We're
  // just waiting for the post-install prompt to show-up, and then we accept (=
  // dismiss) it.
  if (expectUpdate) {
    await waitAppMenuNotificationShown(
      "addon-installed",
      /* addonId */ null,
      /* accept */ true
    );
  } else {
    let results = [];

    let promise = promisePopupNotificationShown("addon-webext-permissions");
    for (let i = 0; i < names.length; i++) {
      let panel = await promise;
      let name = panel.getAttribute("name");
      results.push(name);

      info(`Saw install for ${name}`);
      if (results.length < names.length) {
        info(
          `Waiting for installs for ${names.filter(n => !results.includes(n))}`
        );

        promise = promisePopupNotificationShown("addon-webext-permissions");
      }
      if (confirmInstall) {
        const postInstallPromptShown = waitAppMenuNotificationShown(
          "addon-installed",
          /* addonId */ null,
          /* accept */ true
        );
        panel.button.click();
        await postInstallPromptShown;
      } else {
        panel.secondaryButton.click();
      }
    }

    Assert.deepEqual(results.sort(), names.sort(), "Got expected installs");
  }

  is(
    notificationCount,
    names.length,
    `Saw ${names.length} addon-install-started notification`
  );
  Services.obs.removeObserver(observer, "addon-install-started");
}

add_setup(() => {
  MockFilePicker.init(window.browsingContext);
  registerCleanupFunction(() => {
    MockFilePicker.cleanup();
  });
});

add_task(async function test_install_from_file() {
  let win = await loadInitialView("extension");

  var filePaths = [
    get_addon_file_url("browser_dragdrop1.xpi"),
    get_addon_file_url("browser_dragdrop2.xpi"),
  ];
  for (let uri of filePaths) {
    Assert.notEqual(uri.file, null, `Should have file for ${uri.spec}`);
    ok(uri.file instanceof Ci.nsIFile, `Should have nsIFile for ${uri.spec}`);
  }
  MockFilePicker.setFiles(filePaths.map(aPath => aPath.file));

  // Set handler that executes the core test after the window opens,
  // and resolves the promise when the window closes
  let pInstallURIClosed = checkInstallConfirmation([
    "Drag Drop test 1",
    "Drag Drop test 2",
  ]);

  win.document
    .querySelector('#page-options [action="install-from-file"]')
    .click();

  await pInstallURIClosed;

  await closeView(win);
});

add_task(async function test_install_disabled() {
  const [win, openPageOptions, closePageOptions] = await loadExtensionView();

  await openPageOptions();
  let installButton = win.document.querySelector(
    '[action="install-from-file"]'
  );
  ok(!installButton.hidden, "The install button is shown");
  Assert.equal(
    installButton.getAttribute("data-l10n-id"),
    "addon-install-from-file",
    "got expected l10n ID on the button"
  );
  await closePageOptions();

  await SpecialPowers.pushPrefEnv({ set: [[PREF_XPI_ENABLED, false]] });

  await openPageOptions();
  ok(installButton.hidden, "The install button is now hidden");
  await closePageOptions();

  await SpecialPowers.popPrefEnv();

  await openPageOptions();
  ok(!installButton.hidden, "The install button is shown again");
  await closePageOptions();

  await closeView(win);
});

add_task(async function test_install_from_file_with_pref_set() {
  const addonId = "dragdrop-1@tests.mozilla.org";

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "extensions.webextensions.prefer-update-over-install-for-existing-addon",
        true,
      ],
    ],
  });
  const [win, openPageOptions, closePageOptions] = await loadExtensionView();

  await openPageOptions();
  let installButton = win.document.querySelector(
    '#page-options [action="install-from-file"]'
  );
  Assert.equal(
    installButton.getAttribute("data-l10n-id"),
    "addon-install-or-update-from-file",
    "got expected l10n ID on the button"
  );
  await closePageOptions();

  info("install add-on");
  let fileUrl = get_addon_file_url("browser_dragdrop1.xpi");
  MockFilePicker.setFiles([fileUrl.file]);

  // We verify that the install-from-file indeed installs an XPI file first.
  // Because we want to verify updates after that, we'll want to fully install
  // the add-on by confirming the install.
  let pInstallURIClosed = checkInstallConfirmation(["Drag Drop test 1"], {
    confirmInstall: true,
  });
  let addonStarted = AddonTestUtils.promiseWebExtensionStartup(addonId);
  installButton.click();
  await Promise.all([pInstallURIClosed, addonStarted]);

  let addon = await AddonManager.getAddonByID(addonId);
  ok(!!addon, "Got add-on installed");
  is(addon.version, "1.0", "Got correct add-on version");

  info("update add-on");
  fileUrl = get_addon_file_url("browser_dragdrop1.1.xpi");
  MockFilePicker.setFiles([fileUrl.file]);

  // Now that we have v1.0 of the add-on installed, we're going to update the
  // add-on from an XPI file. The new file doesn't change permissions so the
  // update will take a fast path.
  pInstallURIClosed = checkInstallConfirmation(["Drag Drop test 1.1"], {
    confirmInstall: true,
    expectUpdate: true,
  });
  addonStarted = AddonTestUtils.promiseWebExtensionStartup(addonId);
  installButton.click();
  await Promise.all([pInstallURIClosed, addonStarted]);

  addon = await AddonManager.getAddonByID(addonId);
  is(addon.version, "1.1", "Got correct add-on version after update");

  await closeView(win);
  await addon.uninstall();
  await SpecialPowers.popPrefEnv();
});
