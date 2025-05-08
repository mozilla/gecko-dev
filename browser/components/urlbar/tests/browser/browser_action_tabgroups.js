/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test actions that search and restore tab groups
 */

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
});

add_task(async function test_substring() {
  await simple_tabgroup_search_test("My About Pages", "about");
});

add_task(async function test_words() {
  await simple_tabgroup_search_test("My About Pages", "abou pag");
});
