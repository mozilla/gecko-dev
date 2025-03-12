/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ShoppingUtils: "resource:///modules/ShoppingUtils.sys.mjs",
  ReviewCheckerManager: "resource:///modules/ReviewCheckerManager.sys.mjs",
});

const ACTIVE_PREF = "browser.shopping.experience2023.active";
const AUTO_OPEN_ENABLED_PREF =
  "browser.shopping.experience2023.autoOpen.enabled";
const AUTO_OPEN_USER_ENABLED_PREF =
  "browser.shopping.experience2023.autoOpen.userEnabled";
const AUTO_CLOSE_USER_ENABLED_PREF =
  "browser.shopping.experience2023.autoClose.userEnabled";

function assertSidebarState(isOpen) {
  let { SidebarController } = window;
  let rcSidebarPanelOpen =
    SidebarController.isOpen &&
    SidebarController.currentID == "viewReviewCheckerSidebar";
  Assert.equal(isOpen, rcSidebarPanelOpen);
}

async function testSidebarAutoOpen(testURL, isOpen) {
  let shownPromise =
    isOpen && BrowserTestUtils.waitForEvent(window, "SidebarShown");
  await BrowserTestUtils.withNewTab(testURL, async () => {
    if (isOpen) {
      // If we should be opening the sidebar, wait for it to have
      // sent a shown event.
      await shownPromise;
    } else {
      // Otherwise if nothing will be shown, wait a turn for the
      // change to propagate.
      await TestUtils.waitForTick();
    }

    // Finally, assert the sidebar has the expected open state.
    assertSidebarState(isOpen);

    // Clean up any opened sidebar tools.
    SidebarController.hide();
  });
}

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["browser.shopping.experience2023.integratedSidebar", true],
      ["browser.shopping.experience2023.shoppingSidebar", false],
      ["sidebar.main.tools", "aichat,reviewchecker,syncedtabs,history"],
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  registerCleanupFunction(async () => {
    SidebarController.hide();
  });
});

add_task(async function test_auto_open_rc_sidebar() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  await testSidebarAutoOpen(PRODUCT_TEST_URL, true);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_no_auto_open_for_supported_site() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  await testSidebarAutoOpen(SUPPORTED_SITE_URL, false);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_auto_open_disabled_rc_sidebar() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoOpen.enabled", false],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  await testSidebarAutoOpen(PRODUCT_TEST_URL, false);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_auto_open_user_disabled_rc_sidebar() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", false],
    ],
  });

  await testSidebarAutoOpen(PRODUCT_TEST_URL, false);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_auto_open_not_opted_in_rc_sidebar() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 0],
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  await testSidebarAutoOpen(PRODUCT_TEST_URL, false);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_auto_open_rc_sidebar_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
      ["sidebar.main.tools", "aichat,syncedtabs,history"],
    ],
  });

  await testSidebarAutoOpen(PRODUCT_TEST_URL, false);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_auto_open_rc_sidebar_other_panel_open() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  await SidebarController.show("viewHistorySidebar");

  await testSidebarAutoOpen(PRODUCT_TEST_URL, false);

  SidebarController.hide();

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_auto_open_on_tab_switch() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  let shownPromise = BrowserTestUtils.waitForEvent(window, "SidebarShown");

  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async function (browser) {
    // Wait for the sidebar to auto-open for this tab.
    await shownPromise;

    assertSidebarState(true);

    // Add a new tab
    let newProductTab = BrowserTestUtils.addTab(
      gBrowser,
      OTHER_PRODUCT_TEST_URL
    );
    let newProductBrowser = newProductTab.linkedBrowser;
    await BrowserTestUtils.browserLoaded(
      newProductBrowser,
      false,
      OTHER_PRODUCT_TEST_URL
    );

    // Hide the sidebar.
    SidebarController.hide();

    shownPromise = BrowserTestUtils.waitForEvent(window, "SidebarShown");

    await BrowserTestUtils.switchTab(gBrowser, newProductTab);

    // Wait for the sidebar to show again.
    await shownPromise;

    assertSidebarState(true);

    // Close the sidebar again.
    SidebarController.hide();

    // Switch back to the first tab.
    let firstTab = gBrowser.getTabForBrowser(browser);
    await BrowserTestUtils.switchTab(gBrowser, firstTab);

    // Wait a turn for the change to propagate to make sure nothing is shown.
    await TestUtils.waitForTick();

    assertSidebarState(false);

    await BrowserTestUtils.removeTab(newProductTab);
  });

  await SpecialPowers.popPrefEnv();
});

/* Tests that the sidebar stays open navigating to a product page when auto-close is enabled */
add_task(async function test_auto_close_product_page_rc_sidebar() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoClose.userEnabled", true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    SUPPORTED_SITE_URL,
    async function (browser) {
      await SidebarController.show("viewReviewCheckerSidebar");
      ok(SidebarController.isOpen, "Sidebar is open");

      // Navigate to a new supported site URL:
      BrowserTestUtils.startLoadingURIString(browser, PRODUCT_TEST_URL);
      await BrowserTestUtils.browserLoaded(browser);

      ok(SidebarController.isOpen, "Sidebar should be open");
    }
  );

  SidebarController.hide();

  await SpecialPowers.popPrefEnv();
});

/* Tests that the sidebar stays open navigating to a supported page when auto-close is enabled */
add_task(async function test_auto_close_supported_site_rc_sidebar() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoClose.userEnabled", true],
    ],
  });

  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async function (browser) {
    await SidebarController.show("viewReviewCheckerSidebar");
    ok(SidebarController.isOpen, "Sidebar is open");

    // Navigate to a new supported site URL:
    BrowserTestUtils.startLoadingURIString(browser, SUPPORTED_SITE_URL);
    await BrowserTestUtils.browserLoaded(browser);

    ok(SidebarController.isOpen, "Sidebar should be open");
  });

  SidebarController.hide();

  await SpecialPowers.popPrefEnv();
});

/* Tests that the sidebar closes when navigating to an unsupported site
 *  within the same tab with auto-close enabled
 */
add_task(async function test_auto_close_unsupported_site_rc_sidebar() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoClose.userEnabled", true],
    ],
  });

  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async function (browser) {
    await SidebarController.show("viewReviewCheckerSidebar");
    ok(SidebarController.isOpen, "Sidebar is open");

    // Navigate to a new supported site URL:
    BrowserTestUtils.startLoadingURIString(browser, "about:newtab");
    await BrowserTestUtils.browserLoaded(browser);

    ok(!SidebarController.isOpen, "Sidebar should be closed");
  });

  SidebarController.hide();

  await SpecialPowers.popPrefEnv();
});

/* Tests that the sidebar closes when switching to an unsupported site on a new tab
 *  with auto-close enabled
 */
add_task(async function test_auto_close_on_tab_switch() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.autoClose.userEnabled", true]],
  });

  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async function () {
    // Wait for the sidebar to open
    await SidebarController.show("viewReviewCheckerSidebar");
    // Add a new tab
    let newTab = BrowserTestUtils.addTab(gBrowser, "about:newtab");

    await BrowserTestUtils.switchTab(gBrowser, newTab);

    ok(!SidebarController.isOpen, "Sidebar should be closed");

    await BrowserTestUtils.removeTab(newTab);
  });

  await SpecialPowers.popPrefEnv();
});

/* Tests that the sidebar stays open when navigating to an unsupported site
 *  within the same tab with auto-close disabled
 */

add_task(async function test_auto_close_disabled_unsupported_site_rc_sidebar() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoClose.userEnabled", false],
    ],
  });

  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async function (browser) {
    await SidebarController.show("viewReviewCheckerSidebar");
    ok(SidebarController.isOpen, "Sidebar is open");

    // Navigate to a new supported site URL:
    BrowserTestUtils.startLoadingURIString(browser, "about:newtab");
    await BrowserTestUtils.browserLoaded(browser);

    ok(SidebarController.isOpen, "Sidebar should be open");
  });

  SidebarController.hide();

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_sidebar_stays_closed_when_scrolling() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 1],
      ["browser.shopping.experience2023.autoOpen.enabled", true],
      ["browser.shopping.experience2023.autoOpen.userEnabled", true],
    ],
  });

  const PRODUCT_TEST_URL_WITH_PCS = PRODUCT_TEST_URL + "pcs=1";
  const PRODUCT_TEST_URL_WITH_TH = PRODUCT_TEST_URL + "th=1";

  let shownPromise = BrowserTestUtils.waitForEvent(window, "SidebarShown");
  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async browser => {
    await shownPromise;

    await SpecialPowers.spawn(browser, [PRODUCT_TEST_URL_WITH_PCS], testUrl => {
      content.history.pushState(null, "", testUrl);
    });

    // Sidebar should be shown for a product url
    assertSidebarState(true);

    // Close the sidebar.
    SidebarController.hide();

    // Sidebar should be closed.
    assertSidebarState(false);

    // Mimic scrolling on Amazon.
    await SpecialPowers.spawn(browser, [PRODUCT_TEST_URL_WITH_TH], testUrl => {
      content.history.pushState(null, "", testUrl);
    });

    // Sidebar should have stayed closed.
    assertSidebarState(false);
  });

  await SpecialPowers.popPrefEnv();
});
