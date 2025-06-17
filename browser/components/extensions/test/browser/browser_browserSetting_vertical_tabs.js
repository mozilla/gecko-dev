/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the behaviour of the browserSetting verticalTabs against
 * sidebar verticalTabs.
 */
"use strict";

const SIDEBAR_VERTICAL_TABS = "sidebar.verticalTabs";

const isSidebarVerticalTabs = () => {
  const isEnabled = Services.prefs.getBoolPref(SIDEBAR_VERTICAL_TABS);
  Assert.equal(
    isEnabled,
    CustomizableUI.verticalTabsEnabled,
    "Preference is synced with CustomizableUI bool."
  );
  return isEnabled;
};

add_task(async function test_multiple_extensions_overriding_home_page() {
  function background() {
    browser.test.onMessage.addListener(async (msg, value) => {
      switch (msg) {
        case "tryGet": {
          let getResult = await browser.browserSettings.verticalTabs.get({});
          browser.test.sendMessage("verticalTabsGet", getResult);
          break;
        }
        case "trySet": {
          await browser.browserSettings.verticalTabs.set({
            value,
          });
          browser.test.sendMessage("verticalTabsSet");
          break;
        }
        case "tryClear": {
          await browser.browserSettings.verticalTabs.clear({});
          browser.test.sendMessage("verticalTabsClear");
          break;
        }
      }
    });
  }

  let extObj = {
    manifest: {
      chrome_settings_overrides: {},
      permissions: ["browserSettings"],
    },
    useAddonManager: "temporary",
    background,
  };

  let ext = ExtensionTestUtils.loadExtension(extObj);
  await ext.startup();

  async function checkVerticalTabsGet(expectedValue) {
    ext.sendMessage("tryGet");
    let verticalTabsGet = await ext.awaitMessage("verticalTabsGet");
    is(
      verticalTabsGet.value,
      expectedValue,
      `verticalTabs setting returns the expected value: ${expectedValue}.`
    );
  }

  // Changes through sidebar.verticalTabs are reflected in browserSettings.verticalTabs.
  await SpecialPowers.pushPrefEnv({
    set: [[SIDEBAR_VERTICAL_TABS, false]],
  });

  await checkVerticalTabsGet(false);
  ok(!isSidebarVerticalTabs(), `expect ${SIDEBAR_VERTICAL_TABS} to be false.`);

  await SpecialPowers.pushPrefEnv({
    set: [[SIDEBAR_VERTICAL_TABS, true]],
  });

  await checkVerticalTabsGet(true);
  ok(isSidebarVerticalTabs(), `expect ${SIDEBAR_VERTICAL_TABS} to be true.`);

  await SpecialPowers.pushPrefEnv({
    set: [[SIDEBAR_VERTICAL_TABS, false]],
  });

  await checkVerticalTabsGet(false);
  ok(!isSidebarVerticalTabs(), `expect ${SIDEBAR_VERTICAL_TABS} to be false.`);

  // Changes through browserSettings.verticalTabs are reflected in sidebar.verticalTabs.
  ext.sendMessage("trySet", true);
  await ext.awaitMessage("verticalTabsSet");
  await checkVerticalTabsGet(true);
  ok(isSidebarVerticalTabs(), `expect ${SIDEBAR_VERTICAL_TABS} to be true.`);

  ext.sendMessage("trySet", false);
  await ext.awaitMessage("verticalTabsSet");
  await checkVerticalTabsGet(false);
  ok(!isSidebarVerticalTabs(), `expect ${SIDEBAR_VERTICAL_TABS} to be false.`);

  ext.sendMessage("trySet", true);
  await ext.awaitMessage("verticalTabsSet");

  ext.sendMessage("tryClear");
  await ext.awaitMessage("verticalTabsClear");
  await checkVerticalTabsGet(false);
  ok(!isSidebarVerticalTabs(), `expect ${SIDEBAR_VERTICAL_TABS} to be false.`);

  await ext.unload();
});
