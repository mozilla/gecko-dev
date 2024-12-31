/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const { PERMISSION_L10N } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissionMessages.sys.mjs"
);

AddonTestUtils.initMochitest(this);

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.userScripts.mv3.enabled", true]],
  });
});

function loadExtensionWithPermissions(addonId) {
  function extensionScript() {
    browser.test.onMessage.addListener(async (msg, perm) => {
      browser.test.assertEq("permissions_request", msg, "Expected msg");
      const permissions = [perm];
      let granted = await new Promise(resolve => {
        browser.test.withHandlingUserInput(() => {
          resolve(browser.permissions.request({ permissions }));
        });
      });
      if (perm === "userScripts") {
        browser.test.assertEq(
          granted,
          !!browser.userScripts,
          "userScripts API availability matches permission"
        );
      }
      browser.test.sendMessage("permissions_request:done", granted);
    });
  }
  return ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      manifest_version: 3,
      optional_permissions: ["userScripts", "webNavigation"],
      browser_specific_settings: { gecko: { id: addonId } },
    },
    files: {
      // permissions.request() requires a visible window to attach the prompt
      // to, so it must be in a tab instead of a background script.
      "tab.html": `<!DOCTYPE html><script src="tab.js"></script>`,
      "tab.js": extensionScript,
    },
  });
}

async function triggerPermRequest(extension, perm) {
  const url = `moz-extension://${extension.uuid}/tab.html`;
  let granted;
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async () => {
    extension.sendMessage("permissions_request", perm);
    granted = await extension.awaitMessage("permissions_request:done");
  });
  return granted;
}

function getUserScriptsCheckbox(panel) {
  // popupnotifications has one checkbox (.popup-notification-checkbox) by
  // default, but there should not be anything else, other than potentially
  // the userscript checkbox.
  let checkbox = panel.querySelector(
    "checkbox:not(.popup-notification-checkbox)"
  );
  if (checkbox) {
    is(
      checkbox.textContent,
      PERMISSION_L10N.formatValueSync("webext-perms-description-userScripts"),
      "userScripts permission warning is the label of a checkbox"
    );
  }
  return checkbox;
}

function getUserScriptsWarningBar(panel) {
  let warningBar = panel.querySelector("moz-message-bar[type='warning']");
  if (warningBar) {
    is(
      warningBar.message,
      PERMISSION_L10N.formatValueSync(
        "webext-perms-extra-warning-userScripts-short"
      ),
      "Warning should be the extra description for userScripts"
    );
  }
  return warningBar;
}

function assertIsUserScriptsPermissionPrompt(panel) {
  let checkbox = getUserScriptsCheckbox(panel);
  ok(checkbox, "Found checkbox for userScripts permission");

  let warningBar = getUserScriptsWarningBar(panel);
  ok(warningBar, "Panel contains warning message");
  is(warningBar, checkbox.nextElementSibling, "Warning is below checkbox");
}

function assertIsNotUserScriptPermissionPrompt(panel) {
  is(getUserScriptsCheckbox(panel), null, "Panel does not have a checkbox");
  is(getUserScriptsWarningBar(panel), null, "Panel does not have a warning");
}

function toggleUserScriptCheckbox(panel, { checked, oldChecked = !checked }) {
  let checkbox = getUserScriptsCheckbox(panel);
  is(checkbox.checked, oldChecked, "Initial checkbox state");
  // Simulate user clicking the checkbox to toggle it:
  checkbox.click();
  is(checkbox.checked, checked, "Checked state should change");
}

add_task(async function test_short_prompt_message() {
  // The mocks ( https://bugzilla.mozilla.org/show_bug.cgi?id=1917000#c2 ) show
  // two similar permission messages. The message in the prompt should be
  // shorter than the message in about:addons, because the prompt has less
  // visual room than about:addons.

  // assertIsUserScriptsPermissionPrompt above confirms that this string is
  // used by the prompt:
  let promptWarning = PERMISSION_L10N.formatValueSync(
    "webext-perms-extra-warning-userScripts-short"
  );
  // browser_html_detail_permissions_userScripts.js confirms that this string
  // is used in about:addons:
  let aboutaddonsWarning = PERMISSION_L10N.formatValueSync(
    "webext-perms-extra-warning-userScripts-long"
  );
  ok(promptWarning, "Has short warning");
  ok(aboutaddonsWarning, "Has long warning");
  Assert.greater(
    aboutaddonsWarning.length,
    promptWarning.length,
    "Short warningg should be shorter than the long warning"
  );
});

add_task(async function test_userScripts_not_allowed_by_default() {
  const addonId = "test@test_userScripts_not_allowed_by_default";
  let extension = loadExtensionWithPermissions(addonId);
  await extension.startup();

  async function verifyUserScriptPromptDeniedByDefault(doToggleBeforeDeny) {
    let panelPromise = promisePopupNotificationShown(
      "addon-webext-permissions"
    );
    let resultPromise = triggerPermRequest(extension, "userScripts");
    let panel = await panelPromise;
    assertIsUserScriptsPermissionPrompt(panel);

    ok(panel.button.disabled, "Allow button initially disabled");
    panel.button.click();
    ok(!panel.hidden, "Should have ignored clicks on a disabled Allow button");

    if (doToggleBeforeDeny) {
      // Sanity check: Toggling checkbox before deny does not somehow grant
      // the permission when the prompt is denied.
      toggleUserScriptCheckbox(panel, { checked: true, oldChecked: false });
    }

    panel.secondaryButton.click();
    ok(panel.hidden, "Should be able to Deny the prompt");

    is(await resultPromise, false, "Permission should be denied upon Deny");
  }

  info("Verifying that userScripts permission is denied by default");
  await verifyUserScriptPromptDeniedByDefault();

  info("Verifying that disabled button does not bleed to unrelated permission");
  {
    let panelPromise = promisePopupNotificationShown(
      "addon-webext-permissions"
    );
    let resultPromise = triggerPermRequest(extension, "webNavigation");
    let panel = await panelPromise;
    assertIsNotUserScriptPermissionPrompt(panel);
    is(
      panel.permsSingleEl.textContent,
      PERMISSION_L10N.formatValueSync("webext-perms-description-webNavigation"),
      "Regular permission string can be displayed"
    );
    ok(!panel.button.disabled, "Allow button is not disabled");
    panel.button.click();
    ok(panel.hidden, "Permission prompt closed upon Allow");
    is(await resultPromise, true, "webNavigation permission granted on Allow");
  }

  info("Verifying that another userScripts prompt still behaves as before");
  await verifyUserScriptPromptDeniedByDefault();

  // Sanity check: the Allow button should be the only path towards permission
  // approval. Checking the checkbox and clicking Deny should still result in
  // permission denial.
  info("Verifying that Deny button overrides the checked userScripts checkbox");
  await verifyUserScriptPromptDeniedByDefault(/* doToggleBeforeDeny */ true);

  await extension.unload();
});

add_task(async function test_userScripts_allowed_with_double_confirmation() {
  const addonId = "test@test_userScripts_allowed_with_double_confirmation";
  let extension = loadExtensionWithPermissions(addonId);
  await extension.startup();

  let panelPromise = promisePopupNotificationShown("addon-webext-permissions");
  let resultPromise = triggerPermRequest(extension, "userScripts");
  let panel = await panelPromise;
  assertIsUserScriptsPermissionPrompt(panel);

  ok(panel.button.disabled, "Allow button initially disabled");
  toggleUserScriptCheckbox(panel, { checked: true, oldChecked: false });
  ok(!panel.button.disabled, "Allow button enabled after checking checkbox");
  toggleUserScriptCheckbox(panel, { checked: false });
  ok(panel.button.disabled, "Allow button disabled again after unchecking");

  panel.button.click();
  ok(!panel.hidden, "Should have ignored clicks on a disabled Allow button");

  toggleUserScriptCheckbox(panel, { checked: true });
  ok(!panel.button.disabled, "Allow button enabled again after toggling check");
  panel.button.click();
  ok(panel.hidden, "Should have closed permission prompt on Allow");

  is(await resultPromise, true, "userScripts allowed after double prompt");

  await extension.unload();
});

// The userScripts prompting UI is designed for use with optional permissions
// only because "userScripts" can never appear in an install prompt, due to it
// being an OptionalOnlyPermission. See test_ext_permissions_optional_only.js
// for additional coverage on "userScripts" being an optional-only permission.
add_task(async function test_userScripts_cannot_be_install_time_permission() {
  const addonId = "test@test_userScripts_cannot_be_install_time_permission";

  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      manifest_version: 3,
      // cookies has no permission message, webNavigation does.
      permissions: ["cookies", "userScripts", "webNavigation"],
      browser_specific_settings: { gecko: { id: addonId } },
      // Suppress private browsing checkbox in install prompt.
      incognito: "not_allowed",
    },
  });

  // The unsupported "userScripts" in "permissions" will trigger a warning.
  ExtensionTestUtils.failOnSchemaWarnings(false);

  let startupPromise = AddonTestUtils.promiseWebExtensionStartup(addonId);

  await BrowserTestUtils.withNewTab("about:blank", async () => {
    const panelPromise = promisePopupNotificationShown(
      "addon-webext-permissions"
    );

    gURLBar.value = xpi.path;
    gURLBar.focus();
    EventUtils.synthesizeKey("KEY_Enter");
    let panel = await panelPromise;
    assertIsNotUserScriptPermissionPrompt(panel);
    is(
      panel.permsSingleEl.textContent,
      PERMISSION_L10N.formatValueSync("webext-perms-description-webNavigation"),
      "Install prompt displays the (only) recognized required permission"
    );
    ok(BrowserTestUtils.isHidden(panel.permsListEl), "Perm list is hidden");

    let { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
      // Click button to "Add" extension.
      panel.button.click();
      let extension = await startupPromise;
      ok(extension.hasPermission("webNavigation"), "Has approved permission");
      ok(!extension.hasPermission("userScripts"), "No userScripts permission");
    });
    AddonTestUtils.checkMessages(messages, {
      expected: [
        {
          message:
            /Reading manifest: Warning processing permissions: Error processing permissions.1: Value "userScripts" must either:/,
        },
      ],
    });

    // Remove the post-install doorhanger, because it sticks around (even after
    // the add-on was uninstalled), and may confuse other tests that do not
    // expect the notification to be visible.
    window.AppMenuNotifications.removeNotification("addon-installed");
  });

  let addon = await AddonManager.getAddonByID(addonId);
  await addon.uninstall();

  ExtensionTestUtils.failOnSchemaWarnings(true);
});
