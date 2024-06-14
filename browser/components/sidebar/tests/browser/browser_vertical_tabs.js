/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(() =>
  SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["sidebar.verticalTabs", false],
    ],
  })
);
registerCleanupFunction(() => SpecialPowers.popPrefEnv());

add_task(async function test_toggle_vertical_tabs() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  await waitForBrowserWindowActive(win);
  const { document } = win;
  const sidebar = document.querySelector("sidebar-main");
  ok(sidebar, "Sidebar is shown.");

  let tabStrip = document.getElementById("tabbrowser-tabs");
  let defaultTabstripParent = document.getElementById(
    "TabsToolbar-customization-target"
  );
  let verticalTabs = document.getElementById("vertical-tabs");
  ok(verticalTabs.hidden, "Vertical tabs slot is hidden");

  is(
    tabStrip.parentNode,
    defaultTabstripParent,
    "Tabstrip is in default horizontal position"
  );
  is(
    tabStrip.nextElementSibling.id,
    "new-tab-button",
    "Tabstrip is before the new tab button"
  );

  // flip the pref to move the tabstrip into the sidebar
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.verticalTabs", true]] });
  ok(!verticalTabs.hidden, "Vertical tabs slot is visible");
  is(
    tabStrip.parentNode,
    verticalTabs,
    "Tabstrip is slotted into the sidebar vertical tabs container"
  );
  is(win.gBrowser.tabs.length, 1, "Tabstrip now has one tab");

  // make sure the tab context menu still works
  const contextMenu = document.getElementById("tabContextMenu");
  win.gBrowser.selectedTab.focus();
  EventUtils.synthesizeMouseAtCenter(
    win.gBrowser.selectedTab,
    {
      type: "contextmenu",
      button: 2,
    },
    win
  );

  await openAndWaitForContextMenu(contextMenu, win.gBrowser.selectedTab, () => {
    document.getElementById("context_openANewTab").click();
  });

  is(win.gBrowser.tabs.length, 2, "Tabstrip now has two tabs");

  // flip the pref to move the tabstrip horizontally
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.verticalTabs", false]] });

  ok(verticalTabs.hidden, "Vertical tabs slot is hidden");
  is(
    tabStrip.parentNode,
    defaultTabstripParent,
    "Tabstrip is in default horizontal position"
  );
  is(
    tabStrip.nextElementSibling.id,
    "new-tab-button",
    "Tabstrip is before the new tab button"
  );

  await BrowserTestUtils.closeWindow(win);
});
