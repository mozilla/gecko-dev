/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function loadExtension(options) {
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",

    manifest: {
      permissions: ["tabs"],
      ...options.manifest,
    },

    files: {},

    background: options.background,
  });

  await extension.startup();

  return extension;
}

add_task(async function run_test_shortcuts_tab() {
  const extensionId = "shortcuts@tests.mozilla.org";

  let exampleTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let extension = await loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id: extensionId },
      },
      commands: {
        "toggle-feature": {
          suggested_key: { default: "F1" },
          description: "Send a 'toggle-feature' event to the extension",
        },
      },
    },

    background: async function () {
      try {
        let [firstTab] = await browser.tabs.query({
          currentWindow: true,
          active: true,
        });

        browser.test.log("Open shortcuts page. Expect fresh load.");

        await browser.commands.openShortcutSettings();
        let [shortcutsTab] = await browser.tabs.query({
          currentWindow: true,
          active: true,
        });

        browser.test.assertTrue(
          shortcutsTab.id != firstTab.id,
          "Tab is a new tab"
        );
        browser.test.assertEq(
          "about:addons",
          shortcutsTab.url,
          "Tab contains AddonManager"
        );

        browser.test.log("Switch tabs.");
        await browser.tabs.update(firstTab.id, { active: true });

        browser.test.log("Re-open shortcuts page. Expect tab re-selected.");
        await browser.commands.openShortcutSettings();
        let [reusedTab] = await browser.tabs.query({
          currentWindow: true,
          active: true,
        });

        browser.test.assertEq(
          shortcutsTab.id,
          reusedTab.id,
          "Tab is the same as the previous shortcuts tab"
        );
        browser.test.assertEq(
          "about:addons",
          reusedTab.url,
          "Tab contains AddonManager"
        );

        browser.test.log("Remove shortcuts tab.");
        await browser.tabs.remove(reusedTab.id);

        browser.test.log("Re-open shortcuts page. Expect fresh load.");
        await browser.commands.openShortcutSettings();
        let [reopenedTab] = await browser.tabs.query({
          currentWindow: true,
          active: true,
        });

        browser.test.assertEq(
          "about:addons",
          reopenedTab.url,
          "Tab contains AddonManager"
        );
        browser.test.assertTrue(
          reopenedTab.id != shortcutsTab.id,
          "Tab is a new tab"
        );

        await browser.tabs.remove(firstTab.id);
        await browser.tabs.remove(reopenedTab.id);
      } catch (error) {
        browser.test.fail(`Error: ${error} :: ${error.stack}`);
      }
      browser.test.sendMessage("background-done");
    },
  });

  await extension.awaitMessage("background-done");
  await extension.unload();

  BrowserTestUtils.removeTab(exampleTab);
});

add_task(async function run_test_shortcuts_view() {
  const extensionId1 = "extension1@tests.mozilla.org";
  const extensionId2 = "extension2@tests.mozilla.org";

  let exampleTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let extension1 = await loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id: extensionId1 },
      },
      commands: {
        "toggle-feature": {
          suggested_key: { default: "F1" },
          description: "Send a 'toggle-feature' event to the extension",
        },
      },
    },

    background: function () {
      browser.test.sendMessage("background1-done");
    },
  });

  let extension2 = await loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id: extensionId2 },
      },
      commands: {
        "toggle-feature": {
          suggested_key: { default: "F2" },
          description: "Send a 'toggle-feature' event to the extension",
        },
      },
    },

    background: async function () {
      try {
        await browser.commands.openShortcutSettings();
      } catch (error) {
        browser.test.fail(`Error: ${error} :: ${error.stack}`);
      }
      browser.test.sendMessage("background2-done");
    },
  });

  await Promise.all([
    extension1.awaitMessage("background1-done"),
    extension2.awaitMessage("background2-done"),
  ]);

  const addonState = await SpecialPowers.spawn(
    gBrowser.selectedTab.linkedBrowser,
    [extensionId1, extensionId2],
    (ext1, ext2) => {
      const addonPageHeader =
        content.document.querySelector("addon-page-header");
      const extension1Card = content.document.querySelector(
        `.card.shortcut[addon-id="${ext1}"]`
      );
      const extension2Card = content.document.querySelector(
        `.card.shortcut[addon-id="${ext2}"]`
      );
      return {
        view: addonPageHeader?.getAttribute("current-view"),
        param: addonPageHeader?.getAttribute("current-param"),
        extension1Focused:
          extension1Card?.classList.contains("focused-extension"),
        extension2Focused:
          extension2Card?.classList.contains("focused-extension"),
      };
    }
  );
  is(addonState.view, "shortcuts", "AddonManager rendered shortcuts view");
  is(addonState.param, extensionId2, "AddonManager rendered shortcuts param");
  is(addonState.extension1Focused, false, "Extension 1 not focused");
  is(addonState.extension2Focused, true, "Extension 2 is focused");

  await Promise.all([extension1.unload(), extension2.unload()]);

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(exampleTab);
});

add_task(async function run_test_shortcuts_empty_commands() {
  const extensionId = "noshortcuts@tests.mozilla.org";

  let exampleTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let extension = await loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id: extensionId },
      },
      commands: {},
    },

    background: async function () {
      try {
        await browser.commands.openShortcutSettings();
      } catch (error) {
        browser.test.fail(`Error: ${error} :: ${error.stack}`);
      }
      browser.test.sendMessage("background-done");
    },
  });

  await extension.awaitMessage("background-done");

  const addonState = await SpecialPowers.spawn(
    gBrowser.selectedTab.linkedBrowser,
    [extensionId],
    () => {
      const addonPageHeader =
        content.document.querySelector("addon-page-header");
      return {
        view: addonPageHeader?.getAttribute("current-view"),
        param: addonPageHeader?.getAttribute("current-param"),
      };
    }
  );
  is(addonState.view, "shortcuts", "AddonManager rendered shortcuts view");
  is(addonState.param, extensionId, "AddonManager rendered shortcuts param");

  await extension.unload();

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(exampleTab);
});

add_task(async function run_test_shortcuts_no_commands() {
  const extensionId = "noshortcuts@tests.mozilla.org";

  let exampleTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let extension = await loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id: extensionId },
      },
    },

    background: async function () {
      try {
        browser.test.assertTrue(
          !browser.commands?.openShortcutSettings,
          "openShortcutSettings not defined"
        );
      } catch (error) {
        browser.test.fail(`Error: ${error} :: ${error.stack}`);
      }
      browser.test.sendMessage("background-done");
    },
  });

  await extension.awaitMessage("background-done");
  await extension.unload();

  BrowserTestUtils.removeTab(exampleTab);
});
