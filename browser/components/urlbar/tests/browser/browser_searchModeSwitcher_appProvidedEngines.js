/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the Unified Search Button with app provided engines.
 */

"use strict";

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });
});

add_task(async function test_search_mode_app_provided_engines() {
  let cleanup = await installPersistTestEngines();

  let switcher = document.getElementById("urlbar-searchmode-switcher");
  await BrowserTestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(switcher),
    `Wait until unified search button is visible`
  );

  let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);

  info("Press on the example menu button and enter search mode");
  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(window);
  popup.querySelector("toolbarbutton[label=Example]").click();

  await popupHidden;

  info("Search mode has been changed");
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "Example",
    entry: "searchbutton",
    source: 3,
  });

  info("Press the close button and escape search mode");
  window.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.assertSearchMode(window, null);

  cleanup();
  await resetApplicationProvidedEngines();
});
