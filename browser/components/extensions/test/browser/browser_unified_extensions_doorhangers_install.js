/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

const XPI_REL_PATH = "toolkit/mozapps/extensions/test/xpinstall/amosigned.xpi";
// Installation via https triggers an error in checkCert like bug 1895502,
// so we need to access the XPI over http.
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const XPI_URL = `http://example.com/browser/${XPI_REL_PATH}`;
const XPI_FILE_PATH = getTestFilePath(`../../../../../${XPI_REL_PATH}`);
const XPI_ADDON_ID = "amosigned-xpi@tests.mozilla.org";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      // User activation is usually required to trigger install from navigation.
      // Relax the user input requirements while running this test.
      ["xpinstall.userActivation.required", false],
      // See comment at XPI_URL - need to load over http because of bug 1895502.
      ["dom.security.https_first", false],
    ],
  });
});

async function do_test_addon_install_with_hidden_extensions_button(url) {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.unifiedExtensions.button.always_visible", false]],
  });
  resetExtensionsButtonTelemetry();
  assertExtensionsButtonHidden();

  const permissionsPromptPromise = promisePopupNotificationShown(
    "addon-webext-permissions"
  );
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url,
    // When a file://-URL is navigated to, withNewTab does not resolve unless
    // waitForLoad is set to false.
    waitForLoad: false,
  });

  info(`Waiting for install prompt after navigating to ${url}`);
  const permissionsPrompt = await permissionsPromptPromise;
  assertExtensionsButtonVisible();

  if (url.startsWith("file:")) {
    // No addon_install_doorhanger at this point because download progress
    // notifications ("addons-progress") are only emitted for remote xpi, not
    // local file:-installations.
    assertExtensionsButtonTelemetry({ extension_permission_prompt: 1 });
  } else {
    assertExtensionsButtonTelemetry({
      addon_install_doorhanger: 1,
      extension_permission_prompt: 1,
    });
  }
  resetExtensionsButtonTelemetry();

  permissionsPrompt.button.click();
  const { AppMenuNotifications } = window;
  await TestUtils.waitForCondition(
    () => AppMenuNotifications.activeNotification?.id === "addon-installed",
    "Waiting for post-install doorhanger to be shown on appmenu"
  );
  AppMenuNotifications.removeNotification("addon-installed");
  assertExtensionsButtonHidden();

  BrowserTestUtils.removeTab(tab);

  let addon = await AddonManager.getAddonByID(XPI_ADDON_ID);
  await addon.uninstall();

  assertExtensionsButtonHidden();
  // No more unexpected events beyond what we already checked before.
  assertExtensionsButtonTelemetry({});

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_addon_download_install_when_button_is_hidden() {
  await do_test_addon_install_with_hidden_extensions_button(XPI_URL);
});

add_task(async function test_addon_file_install_when_button_is_hidden() {
  await do_test_addon_install_with_hidden_extensions_button(
    Services.io.newFileURI(new FileUtils.File(XPI_FILE_PATH)).spec
  );
});
