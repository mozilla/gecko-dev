/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// These tests verify that the shopping sidebar is not initialized if the
// user visits a shopping product page while in private browsing mode.

const PRODUCT_PAGE = "https://example.com/product/Y4YM0Z1LL4";

let verifySidebarPanelNotAdded = async win => {
  const { document } = win;
  let sidebar = document.querySelector("sidebar-main");
  await TestUtils.waitForCondition(
    () => sidebar.toolButtons,
    "Sidebar tools have been added."
  );
  let reviewCheckerButton = sidebar.shadowRoot.querySelector(
    "moz-button[view=viewReviewCheckerSidebar]"
  );
  Assert.equal(reviewCheckerButton, null, "Review Checker should not exist.");
};

// If a product page is open in a private window, and the feature is
// preffed on, the sidebar should not be shown in the private
// window (bug 1901979).
add_task(async function test_bug_1901979_pref_toggle_private_windows() {
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let browser = privateWindow.gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, PRODUCT_PAGE);
  await BrowserTestUtils.browserLoaded(browser);

  verifySidebarPanelNotAdded(privateWindow);

  // Flip the prefs to trigger the bug.
  Services.prefs.setBoolPref("browser.shopping.experience2023.enabled", false);
  Services.prefs.setBoolPref("browser.shopping.experience2023.enabled", true);

  // Verify we still haven't displayed the sidebar.
  verifySidebarPanelNotAdded(privateWindow);

  await BrowserTestUtils.closeWindow(privateWindow);
});

add_task(async function test_private_window_does_not_have_integrated_sidebar() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["sidebar.verticalTabs", true],
    ],
  });

  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let browser = privateWindow.gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, PRODUCT_PAGE);
  await BrowserTestUtils.browserLoaded(browser);

  verifySidebarPanelNotAdded(privateWindow);

  await BrowserTestUtils.closeWindow(privateWindow);

  await SpecialPowers.popPrefEnv();
});
