/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOMAIN = "https://example.com/";
const PATH = "browser/browser/components/privatebrowsing/test/browser/";
const TOP_PAGE = DOMAIN + PATH + "empty_file.html";

add_setup(() => SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", true]] }));

add_task(async function test_sidebar_hidden_on_popup() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const { document } = win;

  const sidebar = document.getElementById("sidebar-main");
  await BrowserTestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(sidebar),
    "Sidebar is visible"
  );
  is(sidebar.hidden, false, "Sidebar is shown initially");

  let privateTab = win.gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(privateTab, TOP_PAGE);
  await BrowserTestUtils.browserLoaded(privateTab);

  // Open a popup window
  let popup = BrowserTestUtils.waitForNewWindow();
  await SpecialPowers.spawn(privateTab, [], () => {
    content.window.open("empty_file.html", "_blank", "width=300,height=300");
  });
  popup = await popup;
  ok(!!popup, "Popup shown");

  const popupSidebar = popup.document.getElementById("sidebar-main");
  ok(popupSidebar.hidden, "Sidebar is hidden on popup window");

  const menubar = popup.document.getElementById("viewSidebarMenu");
  ok(
    Array.from(menubar.childNodes).every(
      menuItem => menuItem.getAttribute("disabled") == "true"
    ),
    "All View > Sidebar menu items are disabled on popup"
  );

  await BrowserTestUtils.closeWindow(win);
  popup.close();
});
