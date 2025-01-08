/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONTENT_PAGE = "https://example.com";
const PRODUCT_PAGE = "https://example.com/product/B09TJGHL5F";

function assertEventMatches(gleanEvent, requiredValues) {
  let limitedEvent = Object.assign({}, gleanEvent);
  for (let k of Object.keys(limitedEvent)) {
    if (!requiredValues.hasOwnProperty(k)) {
      delete limitedEvent[k];
    }
  }
  return Assert.deepEqual(limitedEvent, requiredValues);
}

add_task(async function test_shopping_sidebar_displayed() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.shopping.experience2023.active", true]],
  });

  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(PRODUCT_PAGE, async function (browser) {
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    await BrowserTestUtils.waitForMutationCondition(
      shoppingButton,
      {
        attributeFilter: ["shoppingsidebaropen"],
      },
      () => shoppingButton.getAttribute("shoppingsidebaropen") == "true"
    );
    let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");
    Assert.ok(
      BrowserTestUtils.isVisible(sidebar),
      "Sidebar should be visible."
    );

    // open a new tab onto a page where sidebar is not visible.
    let contentTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: CONTENT_PAGE,
    });

    // change the focused tab a few times to ensure we don't increment on tab
    // switch.
    await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);
    await BrowserTestUtils.switchTab(gBrowser, contentTab);
    await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);

    BrowserTestUtils.removeTab(contentTab);
  });

  await Services.fog.testFlushAllChildren();

  var displayedEvents = Glean.shopping.surfaceDisplayed.testGetValue();
  Assert.equal(1, displayedEvents.length);
  assertEventMatches(displayedEvents[0], {
    category: "shopping",
    name: "surface_displayed",
  });

  var addressBarIconDisplayedEvents =
    Glean.shopping.addressBarIconDisplayed.testGetValue();
  assertEventMatches(addressBarIconDisplayedEvents[0], {
    category: "shopping",
    name: "address_bar_icon_displayed",
  });

  // reset FOG and check a page that should NOT have these events
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(CONTENT_PAGE, async function (browser) {
    let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");

    Assert.equal(sidebar, null);
  });

  var emptyDisplayedEvents = Glean.shopping.surfaceDisplayed.testGetValue();
  var emptyAddressBarIconDisplayedEvents =
    Glean.shopping.addressBarIconDisplayed.testGetValue();

  Assert.equal(emptyDisplayedEvents, null);
  Assert.equal(emptyAddressBarIconDisplayedEvents, null);

  // Open a product page in a background tab, verify telemetry is not recorded.
  let backgroundTab = await BrowserTestUtils.addTab(gBrowser, PRODUCT_PAGE);
  await Services.fog.testFlushAllChildren();
  let tabSwitchEvents = Glean.shopping.surfaceDisplayed.testGetValue();
  Assert.equal(tabSwitchEvents, null);
  Services.fog.testResetFOG();

  // Next, switch tabs to the backgrounded product tab and verify telemetry is
  // recorded.
  await BrowserTestUtils.switchTab(gBrowser, backgroundTab);
  await Services.fog.testFlushAllChildren();
  tabSwitchEvents = Glean.shopping.surfaceDisplayed.testGetValue();
  Assert.equal(1, tabSwitchEvents.length);
  assertEventMatches(tabSwitchEvents[0], {
    category: "shopping",
    name: "surface_displayed",
  });
  Services.fog.testResetFOG();

  // Finally, switch tabs again and verify telemetry is not recorded for the
  // background tab after it has been foregrounded once.
  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);
  await BrowserTestUtils.switchTab(gBrowser, backgroundTab);
  await Services.fog.testFlushAllChildren();
  tabSwitchEvents = Glean.shopping.surfaceDisplayed.testGetValue();
  Assert.equal(tabSwitchEvents, null);
  Services.fog.testResetFOG();
  BrowserTestUtils.removeTab(backgroundTab);
  await SpecialPowers.popPrefEnv();
});
