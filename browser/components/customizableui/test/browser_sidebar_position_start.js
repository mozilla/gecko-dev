/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const LEFT_POSITION = 0;
const CUSTOM_POSITION = 2;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.revamp", true],
      ["sidebar.position_start", true],
      ["sidebar.verticalTabs", false],
    ],
  });
  CustomizableUI.reset();
});

registerCleanupFunction(() => {
  CustomizableUI.reset();
});

function assertPositionEnd(message) {
  const widgets = CustomizableUI.getWidgetIdsInArea(CustomizableUI.AREA_NAVBAR);
  Assert.equal(widgets.at(-1), "sidebar-button", message);
}

add_task(async function test_moving_sidebar_updates_button_placement() {
  Assert.equal(
    CustomizableUI.getPlacementOfWidget("sidebar-button").position,
    LEFT_POSITION,
    "By default, the sidebar button is on the left."
  );

  info("Move sidebar to the right.");
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.position_start", false]],
  });
  assertPositionEnd(
    "When the sidebar moves to the right, the toolbar button moves to the right."
  );

  info("Move sidebar back to the left.");
  await SpecialPowers.popPrefEnv();
  Assert.equal(
    CustomizableUI.getPlacementOfWidget("sidebar-button").position,
    LEFT_POSITION,
    "When the sidebar moves back to the left, the toolbar button moves to the left."
  );
});

add_task(async function test_retain_user_customized_button_placement() {
  info("Move sidebar to the right.");
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.position_start", false]],
  });

  info("Move the sidebar button around.");
  CustomizableUI.moveWidgetWithinArea("sidebar-button", CUSTOM_POSITION);

  info("Move sidebar back to the left.");
  await SpecialPowers.popPrefEnv();
  Assert.equal(
    CustomizableUI.getPlacementOfWidget("sidebar-button").position,
    CUSTOM_POSITION,
    "If user has customized the button placement, it does not auto-move."
  );

  CustomizableUI.reset();
});

add_task(async function test_reset_after_moving_sidebar() {
  info("Move sidebar to the right.");
  await SpecialPowers.pushPrefEnv({
    set: [["sidebar.position_start", false]],
  });

  info("Reset CustomizableUI.");
  CustomizableUI.reset();
  Assert.ok(
    Services.prefs.getBoolPref("sidebar.position_start"),
    "After reset, the sidebar is on the left."
  );
  Assert.equal(
    CustomizableUI.getPlacementOfWidget("sidebar-button").position,
    LEFT_POSITION,
    "After reset, the sidebar button is on the left."
  );

  info("Undo CustomizableUI reset.");
  CustomizableUI.undoReset();
  Assert.equal(
    Services.prefs.getBoolPref("sidebar.position_start"),
    false,
    "After undo reset, the sidebar is on the right."
  );
  assertPositionEnd("After undo reset, the sidebar button is on the right.");
});

add_task(async function test_update_placement_vertical_tabs() {
  info("Move sidebar to the right and enable vertical tabs.");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["sidebar.position_start", false],
      ["sidebar.verticalTabs", true],
    ],
  });
  assertPositionEnd(
    "Toolbar button is on the right side of the vertical tabs navbar."
  );
});
