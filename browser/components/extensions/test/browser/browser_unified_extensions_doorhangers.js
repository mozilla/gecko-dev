/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

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

    panel.button.click();
    await ext.awaitMessage("ok");

    await ext.unload();
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
