/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONTENT_PAGE = "https://example.com";
const SHOPPING_SIDEBAR_WIDTH_PREF =
  "browser.shopping.experience2023.sidebarWidth";
const SHOPPING_INTEGRATED_SIDEBAR_PREF =
  "browser.shopping.experience2023.integratedSidebar";

add_task(async function test_sidebar_opens_correct_size() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
      ["browser.shopping.experience2023.active", true],
      [SHOPPING_SIDEBAR_WIDTH_PREF, 0],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: PRODUCT_TEST_URL,
  });

  let browserPanel = gBrowser.getPanel(tab.linkedBrowser);
  let sidebar = browserPanel.querySelector("shopping-sidebar");

  await TestUtils.waitForCondition(() => sidebar.scrollWidth === 320);

  is(sidebar.scrollWidth, 320, "Shopping sidebar should default to 320px");

  let prefChangedPromise = TestUtils.waitForPrefChange(
    SHOPPING_SIDEBAR_WIDTH_PREF
  );
  sidebar.style.width = "345px";
  await TestUtils.waitForCondition(() => sidebar.scrollWidth === 345);
  await prefChangedPromise;

  let shoppingButton = document.getElementById("shopping-sidebar-button");
  shoppingButton.click();
  await BrowserTestUtils.waitForMutationCondition(
    shoppingButton,
    {
      attributeFilter: ["shoppingsidebaropen"],
    },
    () => shoppingButton.getAttribute("shoppingsidebaropen") == "false"
  );

  shoppingButton.click();
  await BrowserTestUtils.waitForMutationCondition(
    shoppingButton,
    {
      attributeFilter: ["shoppingsidebaropen"],
    },
    () => shoppingButton.getAttribute("shoppingsidebaropen") == "true"
  );

  await TestUtils.waitForCondition(() => sidebar.scrollWidth === 345);

  is(
    sidebar.scrollWidth,
    345,
    "Shopping sidebar should open to previous set width of 345"
  );

  gBrowser.removeTab(tab);
});

add_task(async function test_sidebar_and_button_not_present_if_integrated() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
      [SHOPPING_INTEGRATED_SIDEBAR_PREF, false],
    ],
  });

  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    let browserPanel = gBrowser.getPanel(browser);

    BrowserTestUtils.startLoadingURIString(browser, PRODUCT_TEST_URL);
    await BrowserTestUtils.browserLoaded(browser);

    let sidebar = browserPanel.querySelector("shopping-sidebar");

    ok(BrowserTestUtils.isVisible(sidebar), "Shopping sidebar should be open");

    let shoppingButton = document.getElementById("shopping-sidebar-button");
    ok(
      BrowserTestUtils.isVisible(shoppingButton),
      "Shopping Button should be visible"
    );

    let prefChangedPromise = TestUtils.waitForPrefChange(
      SHOPPING_INTEGRATED_SIDEBAR_PREF
    );
    Services.prefs.setBoolPref(SHOPPING_INTEGRATED_SIDEBAR_PREF, true);
    await prefChangedPromise;

    sidebar = browserPanel.querySelector("shopping-sidebar");
    is(sidebar, null, "Shopping Sidebar should be removed");

    shoppingButton = document.getElementById("shopping-sidebar-button");
    ok(
      BrowserTestUtils.isHidden(shoppingButton),
      "Shopping Button should be hidden"
    );

    prefChangedPromise = TestUtils.waitForPrefChange(
      SHOPPING_INTEGRATED_SIDEBAR_PREF
    );
    Services.prefs.setBoolPref(SHOPPING_INTEGRATED_SIDEBAR_PREF, false);
    await prefChangedPromise;

    sidebar = browserPanel.querySelector("shopping-sidebar");
    ok(BrowserTestUtils.isVisible(sidebar), "Shopping sidebar should be open");

    shoppingButton = document.getElementById("shopping-sidebar-button");
    ok(
      BrowserTestUtils.isVisible(shoppingButton),
      "Shopping Button should be visible"
    );
  });

  Services.prefs.clearUserPref(SHOPPING_INTEGRATED_SIDEBAR_PREF);
  await SpecialPowers.popPrefEnv();
});
