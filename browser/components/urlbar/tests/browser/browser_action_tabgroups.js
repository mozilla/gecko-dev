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

add_task(async function test_basic_restore_tabgroup() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  let aboutRobotsTab = BrowserTestUtils.addTab(win.gBrowser, "about:robots");
  let aboutMozillaTab = BrowserTestUtils.addTab(win.gBrowser, "about:mozilla");
  let tabGroup = win.gBrowser.addTabGroup([aboutRobotsTab, aboutMozillaTab], {
    color: "blue",
    label: "about pages",
  });
  tabGroup.collapsed = true;

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "about",
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
});
