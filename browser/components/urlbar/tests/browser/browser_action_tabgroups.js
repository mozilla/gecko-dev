/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test actions that search and restore tab groups
 */

ChromeUtils.defineESModuleGetters(this, {
  TabGroupTestUtils: "resource://testing-common/TabGroupTestUtils.sys.mjs",
});

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.groups.enabled", true],
      ["browser.urlbar.scotchBonnet.enableOverride", true],
    ],
  });
});

async function simple_tabgroup_search_test(label, searchString) {
  info(`Attempting to find tab group '${label}' by typing '${searchString}'`);
  const win = await BrowserTestUtils.openNewBrowserWindow();
  let aboutRobotsTab = BrowserTestUtils.addTab(win.gBrowser, "about:robots");
  let aboutMozillaTab = BrowserTestUtils.addTab(win.gBrowser, "about:mozilla");
  let tabGroup = win.gBrowser.addTabGroup([aboutRobotsTab, aboutMozillaTab], {
    color: "blue",
    label,
  });
  tabGroup.collapsed = true;

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: searchString,
  });
  await UrlbarTestUtils.promisePopupClose(win, () => {
    EventUtils.synthesizeKey("KEY_Tab", {}, win);
    EventUtils.synthesizeKey("KEY_Enter", {}, win);
  });

  Assert.ok(
    !tabGroup.collapsed,
    "tab group collapsed state should be restored"
  );

  await BrowserTestUtils.closeWindow(win);
}

add_task(async function test_first_letter() {
  await simple_tabgroup_search_test("About Pages", "a");
  TabGroupTestUtils.forgetSavedTabGroups();
});

add_task(async function test_substring() {
  await simple_tabgroup_search_test("My About Pages", "about");
  TabGroupTestUtils.forgetSavedTabGroups();
});

add_task(async function test_words() {
  await simple_tabgroup_search_test("My About Pages", "abou pag");
  TabGroupTestUtils.forgetSavedTabGroups();
});

/**
 * Returns the first ActionsResult produced by the ActionsProviderTabGroups,
 * if present in the search results.
 *
 * @param {UrlbarQueryContext} queryContext
 * @returns {ActionsResult|undefined}
 */
function getTabGroupResult(queryContext) {
  const firstAction = queryContext.results.find(
    result => result.source == UrlbarUtils.RESULT_SOURCE.ACTIONS
  );
  return firstAction?.payload.actionsResults.find(actionResult =>
    actionResult.key.startsWith("tabgroup-")
  );
}

add_task(async function test_private_window() {
  info(
    "creating one open tab group and one saved tab group from a normal window"
  );
  const win = await BrowserTestUtils.openNewBrowserWindow();
  let aboutRobotsTab = BrowserTestUtils.addTab(win.gBrowser, "about:robots");
  let aboutMozillaTab = BrowserTestUtils.addTab(win.gBrowser, "about:mozilla");
  let openTabGroupNormalWindow = win.gBrowser.addTabGroup([aboutRobotsTab], {
    color: "blue",
    label: "robots",
  });
  let savedTabGroup = win.gBrowser.addTabGroup([aboutMozillaTab], {
    color: "red",
    label: "mozilla",
  });
  await TabGroupTestUtils.saveAndCloseTabGroup(savedTabGroup);

  info("creating a private window with one new tab group inside");
  const privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let aboutAboutTab = BrowserTestUtils.addTab(
    privateWin.gBrowser,
    "about:about"
  );
  let privateTabGroup = privateWin.gBrowser.addTabGroup([aboutAboutTab], {
    color: "purple",
    label: "meta",
  });

  const openTabGroupResults =
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: privateWin,
      value: openTabGroupNormalWindow.name,
    });
  Assert.ok(
    !getTabGroupResult(openTabGroupResults),
    "Open tab groups in normal windows should not be accessible from a private window"
  );

  const savedTabGroupResults =
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: privateWin,
      value: savedTabGroup.name,
    });

  Assert.ok(
    !getTabGroupResult(savedTabGroupResults),
    "Saved tab groups should not be accessible from a private window"
  );

  const privateTabGroupResults =
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: privateWin,
      value: privateTabGroup.name,
    });
  Assert.ok(
    getTabGroupResult(privateTabGroupResults),
    "A tab group in a private window should be accessible from a private window"
  );

  await BrowserTestUtils.closeWindow(privateWin);
  await BrowserTestUtils.closeWindow(win);
  TabGroupTestUtils.forgetSavedTabGroups();
});

add_task(async function test_tabs_mode() {
  await simple_tabgroup_search_test("About Pages", "% a");
});
