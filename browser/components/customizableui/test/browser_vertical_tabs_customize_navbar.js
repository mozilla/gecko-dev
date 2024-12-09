/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kPrefCustomizationNavBarWhenVerticalTabs =
  "browser.uiCustomization.verticalNavBar";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.verticalTabs", true]],
  });
});

registerCleanupFunction(async () => {
  await SpecialPowers.popPrefEnv();
  Services.prefs.clearUserPref(kPrefCustomizationNavBarWhenVerticalTabs);
  gBrowser.removeAllTabsBut(gBrowser.tabs[0]);
});

// When switching to vertical tabs, the nav-bar customizations remain the same
// as when in horizontal tabs mode, with the addition of tab strip widgets
// This test asserts we remember any tab strip widget customizations in the nav-bar
// when switching between vertical and horizontal modes
add_task(async function () {
  await startCustomizing();
  is(gBrowser.tabs.length, 2, "Should have 2 tabs");

  let nonCustomizingTab = gBrowser.tabContainer.querySelector(
    "tab:not([customizemode=true])"
  );
  let finishedCustomizing = BrowserTestUtils.waitForEvent(
    gNavToolbox,
    "aftercustomization"
  );

  let alltabsPlacement = CustomizableUI.getPlacementOfWidget("alltabs-button");
  let firefoxViewPlacement = CustomizableUI.getPlacementOfWidget(
    "firefox-view-button"
  );
  is(
    alltabsPlacement.position,
    14,
    "alltabs-button is in its original default position"
  );
  is(
    firefoxViewPlacement.position,
    13,
    "firefox-view-button is in its original default position"
  );

  CustomizableUI.moveWidgetWithinArea("alltabs-button", 1);
  CustomizableUI.moveWidgetWithinArea("firefox-view-button", 2);

  await BrowserTestUtils.switchTab(gBrowser, nonCustomizingTab);
  await finishedCustomizing;

  alltabsPlacement = CustomizableUI.getPlacementOfWidget("alltabs-button");
  firefoxViewPlacement = CustomizableUI.getPlacementOfWidget(
    "firefox-view-button"
  );
  is(alltabsPlacement.area, "nav-bar", "alltabs-button is in the nav-bar");
  is(
    firefoxViewPlacement.area,
    "nav-bar",
    "firefox-view-button is in the nav-bar"
  );
  is(
    alltabsPlacement.position,
    1,
    "alltabs-button is in its new custom position"
  );
  is(
    firefoxViewPlacement.position,
    2,
    "firefox-view-button is in its new custom position"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.verticalTabs", false]],
  });

  let horizontalAlltabsPlacement =
    CustomizableUI.getPlacementOfWidget("alltabs-button");
  let horizontalFirefoxViewPlacement = CustomizableUI.getPlacementOfWidget(
    "firefox-view-button"
  );
  is(
    horizontalAlltabsPlacement.area,
    "TabsToolbar",
    "alltabs-button is in the TabsToolbar"
  );
  is(
    horizontalFirefoxViewPlacement.area,
    "TabsToolbar",
    "firefox-view-button is in the TabsToolbar"
  );
  is(
    horizontalAlltabsPlacement.position,
    3,
    "alltabs-button is in its default horizontal mode position"
  );
  is(
    horizontalFirefoxViewPlacement.position,
    0,
    "firefox-view-button is in its default horizontal mode position"
  );

  // Switching from vertical to horizontal and back to vertical, the customization should be remembered
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.verticalTabs", true]],
  });

  let newAlltabsPlacement =
    CustomizableUI.getPlacementOfWidget("alltabs-button");
  let newFirefoxViewPlacement = CustomizableUI.getPlacementOfWidget(
    "firefox-view-button"
  );
  is(newAlltabsPlacement.area, "nav-bar", "alltabs-button is in the nav-bar");
  is(
    newFirefoxViewPlacement.area,
    "nav-bar",
    "firefox-view-button is in the nav-bar"
  );
  is(
    newAlltabsPlacement.position,
    1,
    "alltabs-button is in its new custom position"
  );
  is(
    newFirefoxViewPlacement.position,
    2,
    "firefox-view-button is in its new custom position"
  );
});
