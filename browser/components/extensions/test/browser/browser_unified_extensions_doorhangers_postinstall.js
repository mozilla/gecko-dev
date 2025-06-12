/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Note: Although the postinstall dialog is currently anchored to the appmenu,
// bug 1910405 tracks anchoring it to the Extensions Button instead.

loadTestSubscript("head_unified_extensions.js");

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

function promisePostInstallNotificationShown(win = window) {
  const { AppMenuNotifications, PanelUI, document } = win;
  return new Promise(resolve => {
    function popupshown() {
      let notification = AppMenuNotifications.activeNotification;
      if (!notification) {
        return;
      }

      is(notification.id, "addon-installed", "Postinstall panel shown");
      ok(PanelUI.isNotificationPanelOpen, "notification panel open");

      PanelUI.notificationPanel.removeEventListener("popupshown", popupshown);

      let popupnotificationID = PanelUI._getPopupId(notification);
      let popupnotification = document.getElementById(popupnotificationID);

      resolve(popupnotification);
    }
    PanelUI.notificationPanel.addEventListener("popupshown", popupshown);
  });
}

async function testAddonPostInstall({
  win = window,
  extensionData,
  grantPrivateBrowsingAllowed = false,
  verifyPostInstallCheckbox,
}) {
  const addonId = extensionData.manifest.browser_specific_settings.gecko.id;
  const xpi = AddonTestUtils.createTempWebExtensionFile(extensionData);
  await BrowserTestUtils.withNewTab({ gBrowser: win.gBrowser }, async () => {
    const permPromise = promisePopupNotificationShown(
      "addon-webext-permissions",
      win
    );
    const postInstallPanelPromise = promisePostInstallNotificationShown(win);
    win.gURLBar.value = xpi.path;
    win.gURLBar.focus();
    EventUtils.synthesizeKey("KEY_Enter", {}, win);
    info(`Waiting for install of ${xpi.path} to show permission prompt`);
    const permPanel = await permPromise;
    if (grantPrivateBrowsingAllowed) {
      let privateBrowsingCheckbox = permPanel.querySelector(
        ".webext-perm-privatebrowsing > moz-checkbox"
      );
      ok(!privateBrowsingCheckbox.checked, "Private browsing off by default");
      privateBrowsingCheckbox.click();
      ok(privateBrowsingCheckbox.checked, "Private browsing access enabled");
    }
    permPanel.button.click();
    info(`Waiting for install of ${xpi.path} to complete`);
    const popupnotification = await postInstallPanelPromise;
    const checkbox = popupnotification.querySelector(
      "#addon-pin-toolbarbutton-checkbox"
    );
    ok(checkbox, "Found 'Pin extension to toolbar' checkbox");
    await verifyPostInstallCheckbox(checkbox);

    info(`Dismissing postinstall notification`);
    await new Promise(resolve => {
      win.PanelUI.notificationPanel.addEventListener("popuphidden", resolve, {
        once: true,
      });
      popupnotification.button.click();
    });
  });
  const addon = await AddonManager.getAddonByID(addonId);
  await addon.uninstall();
}

add_task(async function test_no_pin_without_browser_action() {
  await testAddonPostInstall({
    extensionData: {
      manifest: {
        browser_specific_settings: { gecko: { id: "test@no-button" } },
      },
    },
    verifyPostInstallCheckbox(checkbox) {
      ok(
        BrowserTestUtils.isHidden(checkbox),
        "No pin option for extension without browser action"
      );
    },
  });
});

add_task(async function test_pin_browser_action() {
  await testAddonPostInstall({
    extensionData: {
      manifest: {
        browser_action: {},
        browser_specific_settings: { gecko: { id: "test@with-button" } },
      },
    },
    verifyPostInstallCheckbox(checkbox) {
      ok(
        BrowserTestUtils.isVisible(checkbox),
        "Has pin option for extension with browser action"
      );
      is(checkbox.checked, false, "Not pinned on toolbar by default");
    },
  });
});

// With default_area: "navbar", the checkbox should be checked by default.
add_task(async function test_pin_browser_action_default_area_navbar() {
  await testAddonPostInstall({
    extensionData: {
      manifest: {
        browser_action: { default_area: "navbar" },
        browser_specific_settings: { gecko: { id: "test@default-navbar" } },
      },
    },
    verifyPostInstallCheckbox(checkbox) {
      ok(
        BrowserTestUtils.isVisible(checkbox),
        "Has pin option for extension with browser action"
      );
      is(checkbox.checked, true, "Pinned by default_area: navbar");
      // Note: many other tests already check whether default_area causes the
      // button to appear in the nav-bar, so we skip checking that here.
    },
  });
});

// With default_area: "tabstrip", the checkbox should be invisible, because it
// is not on the toolbar, nor on the Extensions Panel.
add_task(async function test_pin_browser_action_default_area_tabstrip() {
  await testAddonPostInstall({
    extensionData: {
      manifest: {
        browser_action: { default_area: "tabstrip" },
        browser_specific_settings: { gecko: { id: "test@default-tabstrip" } },
      },
    },
    verifyPostInstallCheckbox(checkbox) {
      ok(
        BrowserTestUtils.isHidden(checkbox),
        "No pin option with browser_action.default_area set to non-toolbar"
      );
    },
  });
});

// With default_area: "tabstrip", the checkbox should be visible if vertical
// tabs are enabled, because "tabstrip" maps to the default (Extensions Panel)
// area ("menupanel" internally) in that case, as seen at
// https://searchfox.org/mozilla-central/rev/c18faaae88/toolkit/components/extensions/ExtensionActions.sys.mjs#518-519,523-524,529
add_task(async function test_pin_browser_action_tabstrip_vertical_tabs() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.verticalTabs", true],
      // Although the behavior of interest is toggled via sidebar.verticalTabs,
      // we also need to set sidebar.revamp=true, because enabling verticalTabs
      // also sets revamp=true. By tracking the current value of revamp here,
      // we ensure that the pref is reset to its previous value in popPrefEnv.
      // Not doing this can cause random test failures, see bug 1967959.
      ["sidebar.revamp", true],
    ],
  });
  await testAddonPostInstall({
    extensionData: {
      manifest: {
        browser_action: { default_area: "tabstrip" },
        browser_specific_settings: { gecko: { id: "test@default-tabstrip2" } },
      },
    },
    verifyPostInstallCheckbox(checkbox) {
      ok(
        BrowserTestUtils.isVisible(checkbox),
        "Has pin option when tabstrip area is mapped to Extensions Panel"
      );
      is(checkbox.checked, false, "Unpinned when button is inside panel");
    },
  });
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_private_browsing_no_checkbox_without_perm() {
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  await SimpleTest.promiseFocus(win);
  await testAddonPostInstall({
    win,
    extensionData: {
      manifest: {
        browser_action: {},
        browser_specific_settings: { gecko: { id: "test@no-private-access" } },
      },
    },
    verifyPostInstallCheckbox(checkbox) {
      ok(
        BrowserTestUtils.isHidden(checkbox),
        "Pin option is hidden in private window"
      );
    },
  });
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_private_browsing_checkbox_with_perm() {
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  await SimpleTest.promiseFocus(win);
  await testAddonPostInstall({
    win,
    extensionData: {
      manifest: {
        browser_action: {},
        browser_specific_settings: { gecko: { id: "test@with-private-perm" } },
      },
    },
    grantPrivateBrowsingAllowed: true,
    verifyPostInstallCheckbox(checkbox) {
      ok(
        BrowserTestUtils.isVisible(checkbox),
        "Pin option is shown in private window"
      );
      is(checkbox.checked, false, "Not pinned on toolbar by default");
    },
  });
  await BrowserTestUtils.closeWindow(win);
});

// Test that the checkbox state can be toggled, and that the state propagates
// to all windows.
add_task(async function test_toggle_pin_toolbar() {
  const otherWindow = await BrowserTestUtils.openNewBrowserWindow({});
  await SimpleTest.promiseFocus(otherWindow);
  const frontWindow = await BrowserTestUtils.openNewBrowserWindow({});
  await SimpleTest.promiseFocus(frontWindow);
  const otherPromise = promisePostInstallNotificationShown(otherWindow);
  const ADDON_ID = "test@toggle-my-buttons";
  const widgetId = `${makeWidgetId(ADDON_ID)}-browser-action`;
  await testAddonPostInstall({
    win: frontWindow,
    extensionData: {
      manifest: {
        browser_action: {},
        browser_specific_settings: { gecko: { id: ADDON_ID } },
      },
    },
    async verifyPostInstallCheckbox(checkbox) {
      ok(
        BrowserTestUtils.isVisible(checkbox),
        "Has pin option for extension with browser action"
      );

      // Toggle three times and verify the state each time.
      checkbox.click();
      is(checkbox.checked, true, "Checked after toggling checkbox (1/3)");
      is(
        CustomizableUI.getPlacementOfWidget(widgetId).area,
        CustomizableUI.AREA_NAVBAR,
        "Button should be placed in nav-bar after checking checkbox"
      );

      checkbox.click();
      is(checkbox.checked, false, "Unchecked after toggling checkbox (2/3)");
      is(
        CustomizableUI.getPlacementOfWidget(widgetId).area,
        CustomizableUI.AREA_ADDONS,
        "Button should be placed in Extensions Panel after unchecking checkbox"
      );

      checkbox.click();
      is(checkbox.checked, true, "Checked after toggling checkbox (3/3)");
      is(
        CustomizableUI.getPlacementOfWidget(widgetId).area,
        CustomizableUI.AREA_NAVBAR,
        "Button should be placed in nav-bar after checking checkbox again"
      );

      // Now that we've changed to a non-default state, verify that this state
      // is also observed in other windows.
      {
        await SimpleTest.promiseFocus(otherWindow);
        const otherPopupnotification = await otherPromise;
        const otherCheckbox = otherPopupnotification.querySelector(
          "#addon-pin-toolbarbutton-checkbox"
        );
        ok(
          BrowserTestUtils.isVisible(otherCheckbox),
          "Has pin option in postinstall doorhanger in other window"
        );
        is(checkbox.checked, true, "Checked by last change in frontWindow");
        is(
          CustomizableUI.getPlacementOfWidget(widgetId).area,
          CustomizableUI.AREA_NAVBAR,
          "Button should be placed in nav-bar by change in frontWindow"
        );
        otherCheckbox.click();
        is(otherCheckbox.checked, false, "Can uncheck in other window");
        is(
          CustomizableUI.getPlacementOfWidget(widgetId).area,
          CustomizableUI.AREA_ADDONS,
          "Button should be placed in Extensions Panel in other window"
        );

        await BrowserTestUtils.closeWindow(otherWindow);
      }

      await SimpleTest.promiseFocus(frontWindow);
      is(checkbox.checked, false, "Unchecked by change in other window");
      is(
        CustomizableUI.getPlacementOfWidget(widgetId).area,
        CustomizableUI.AREA_ADDONS,
        "Button should be placed in Extensions Panel in original window"
      );
    },
  });
  await BrowserTestUtils.closeWindow(frontWindow);
});
