/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

registerCleanupFunction(async () => {
  await SpecialPowers.popPrefEnv();
});

add_task(
  async function reset_defaults_should_include_sidebar_button_in_revamp() {
    console.log(CustomizableUI.getPlacementOfWidget("sidebar-button"));
    ok(
      !CustomizableUI.getPlacementOfWidget("sidebar-button"),
      "Sidebar button is not in the nav bar initially"
    );
    CustomizableUI.addWidgetToArea("sidebar-button", "nav-bar");
    is(
      CustomizableUI.getPlacementOfWidget("sidebar-button").area,
      CustomizableUI.AREA_NAVBAR,
      "Sidebar button is in the nav bar"
    );
    CustomizableUI.removeWidgetFromArea("sidebar-button");
    ok(
      !CustomizableUI.getPlacementOfWidget("sidebar-button"),
      "Sidebar button has been removed from the nav bar"
    );
    CustomizableUI.reset();
    ok(
      !CustomizableUI.getPlacementOfWidget("sidebar-button"),
      "Sidebar button has not been restored to the nav bar"
    );

    await SpecialPowers.pushPrefEnv({
      set: [["sidebar.revamp", true]],
    });
    is(
      CustomizableUI.getPlacementOfWidget("sidebar-button").area,
      CustomizableUI.AREA_NAVBAR,
      "Sidebar button is in the nav bar when revamp pref is flipped"
    );
    CustomizableUI.removeWidgetFromArea("sidebar-button");
    ok(
      !CustomizableUI.getPlacementOfWidget("sidebar-button"),
      "Sidebar button has been removed from the nav bar"
    );
    CustomizableUI.reset();
    is(
      CustomizableUI.getPlacementOfWidget("sidebar-button").area,
      CustomizableUI.AREA_NAVBAR,
      "Sidebar button has been restored to the nav bar"
    );
  }
);
