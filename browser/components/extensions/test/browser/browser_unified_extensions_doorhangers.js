/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

ChromeUtils.defineESModuleGetters(this, {
  ExtensionControlledPopup:
    "resource:///modules/ExtensionControlledPopup.sys.mjs",
});

const verifyPermissionsPrompt = async expectAlwaysShown => {
  const ext = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: "some search name",
          search_url: "https://example.com/?q={searchTerms}",
          is_default: true,
        },
      },
      optional_permissions: ["history"],
    },

    background: () => {
      browser.test.onMessage.addListener(async msg => {
        if (msg !== "create-tab") {
          return;
        }

        await browser.tabs.create({
          url: browser.runtime.getURL("content.html"),
          active: true,
        });
      });
    },

    files: {
      "content.html": `<!DOCTYPE html><script src="content.js"></script>`,
      "content.js": async () => {
        browser.test.onMessage.addListener(async msg => {
          browser.test.assertEq(
            msg,
            "grant-permission",
            "expected message to grant permission"
          );

          const granted = await new Promise(resolve => {
            browser.test.withHandlingUserInput(() => {
              resolve(
                browser.permissions.request({ permissions: ["history"] })
              );
            });
          });
          browser.test.assertTrue(granted, "permission request succeeded");

          browser.test.sendMessage("ok");
        });

        browser.test.sendMessage("ready");
      },
    },
  });

  await BrowserTestUtils.withNewTab({ gBrowser }, async () => {
    resetExtensionsButtonTelemetry();
    const defaultSearchPopupPromise = promisePopupNotificationShown(
      "addon-webext-defaultsearch"
    );
    let [panel] = await Promise.all([defaultSearchPopupPromise, ext.startup()]);
    ok(panel, "expected panel");
    let notification = PopupNotifications.getNotification(
      "addon-webext-defaultsearch"
    );
    ok(notification, "expected notification");
    // We always want the defaultsearch popup to be anchored on the urlbar (the
    // ID below) because the post-install popup would be displayed on top of
    // this one otherwise, see Bug 1789407.
    is(
      notification?.anchorElement?.id,
      "addons-notification-icon",
      "expected the right anchor ID for the defaultsearch popup"
    );
    if (expectAlwaysShown) {
      assertExtensionsButtonVisible();
    } else {
      assertExtensionsButtonHidden();
    }
    // Accept to override the search.
    panel.button.click();
    await TestUtils.topicObserved("webextension-defaultsearch-prompt-response");

    ext.sendMessage("create-tab");
    await ext.awaitMessage("ready");

    const popupPromise = promisePopupNotificationShown(
      "addon-webext-permissions"
    );
    ext.sendMessage("grant-permission");
    panel = await popupPromise;
    ok(panel, "expected panel");
    notification = PopupNotifications.getNotification(
      "addon-webext-permissions"
    );
    ok(notification, "expected notification");
    is(
      // We access the parent element because the anchor is on the icon (inside
      // the button), not on the unified extensions button itself.
      notification.anchorElement.id ||
        notification.anchorElement.parentElement.id,
      "unified-extensions-button",
      "expected the right anchor ID"
    );
    assertExtensionsButtonVisible();
    if (expectAlwaysShown) {
      assertExtensionsButtonTelemetry({ extension_permission_prompt: 0 });
    } else {
      assertExtensionsButtonTelemetry({ extension_permission_prompt: 1 });
      resetExtensionsButtonTelemetry();
    }

    panel.button.click();
    await ext.awaitMessage("ok");

    await ext.unload();
    // No more counters beyond the ones explicitly checked.
    assertExtensionsButtonTelemetry({});
  });
};

add_task(async function test_permissions_prompt() {
  await verifyPermissionsPrompt(/* expectAlwaysShown */ true);
});

// This test confirms that the Extensions Button becomes temporarily visible
// when a permission prompt needs to be anchored to it.
add_task(async function test_permissions_prompt_when_button_is_hidden() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.unifiedExtensions.button.always_visible", false]],
  });

  assertExtensionsButtonHidden();

  await verifyPermissionsPrompt(/* expectAlwaysShown */ false);

  info("After install is done, Extensions button should be hidden again");
  assertExtensionsButtonHidden();

  await SpecialPowers.popPrefEnv();
});

// This test confirms that the Extensions Button becomes temporarily visible
// when an ExtensionControlledPopup notification wants to anchor to it.
add_task(async function test_homepage_doorhanger() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.unifiedExtensions.button.always_visible", false]],
  });
  resetExtensionsButtonTelemetry();

  const extension = ExtensionTestUtils.loadExtension({
    manifest: { chrome_settings_overrides: { homepage: "exthome.html" } },
    files: { "exthome.html": "<h1>Extension-defined homepage</h1>" },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let panel = ExtensionControlledPopup._getAndMaybeCreatePanel(document);
  let popupShown = promisePopupShown(panel);
  BrowserCommands.home();

  info("Waiting for 'Your homepage has changed' doorhanger to appear");
  await popupShown;
  assertExtensionsButtonVisible();
  assertExtensionsButtonTelemetry({ extension_controlled_setting: 1 });

  let popupnotification = document.getElementById(
    "extension-homepage-notification"
  );

  // Now close doorhanger and verify that the button is hidden again.
  let popupHidden = promisePopupHidden(panel);
  popupnotification.button.click();
  await popupHidden;

  assertExtensionsButtonHidden();
  // Still the same count as before.
  assertExtensionsButtonTelemetry({ extension_controlled_setting: 1 });

  await extension.unload();

  await SpecialPowers.popPrefEnv();
});
