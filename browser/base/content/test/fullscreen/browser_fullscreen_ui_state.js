/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function check_states(fullscreen) {
  // These are the menu items in the View menu
  const fsEnableMac = document.getElementById("enterFullScreenItem");
  const fsDisableMac = document.getElementById("exitFullScreenItem");
  const fullScreenItem = document.getElementById("fullScreenItem");

  const toolbarButton = document.getElementById("fullscreen-button");

  if (AppConstants.platform == "macosx") {
    ok(
      !fsEnableMac.hasAttribute("checked"),
      "On MacOS, the Enter Full Screen menu item should never get a checkmark"
    );
    ok(
      !fsDisableMac.hasAttribute("checked"),
      "On MacOS, the Exit Full Screen menu item should never get a checkmark"
    );
    is(
      fsEnableMac.hasAttribute("hidden"),
      fullscreen,
      "Enter Full Screen should be visible iff not in full screen mode"
    );
    is(
      fsDisableMac.hasAttribute("hidden"),
      !fullscreen,
      "Exit Full Screen should be visible iff in full screen mode"
    );
  } else {
    is(
      fullScreenItem.hasAttribute("checked"),
      fullscreen,
      "On non-mac platforms, the menu item should be checked iff in full screen mode"
    );
  }
  is(
    toolbarButton.hasAttribute("checked"),
    fullscreen,
    "The toolbar button must be checked iff in full screen mode"
  );

  // And there is little button in the hamburger menu
  let menuShown = BrowserTestUtils.waitForEvent(PanelUI.panel, "ViewShown");
  PanelUI.show();
  await menuShown;
  let hamburgerMenuButton = window.document.getElementById(
    "appMenu-fullscreen-button2"
  );
  is(
    hamburgerMenuButton.hasAttribute("checked"),
    fullscreen,
    "The hambuger menu button should be checked iff in full screen mode"
  );
  let menuHidden = BrowserTestUtils.waitForEvent(PanelUI.panel, "popuphidden");
  PanelUI.hide();
  await menuHidden;
}

async function toggle_fs() {
  let oldFSState = window.fullScreen;
  const fullScreenEntered = BrowserTestUtils.waitForEvent(window, "fullscreen");
  document.getElementById("View:FullScreen").doCommand();
  info(
    `Now waiting for fullscreen mode to be ${oldFSState ? "exited" : "entered"}`
  );
  await fullScreenEntered;
  return window.fullScreen;
}

add_task(async function test_fullscreen_ui_state() {
  // We add the custom toolbar button to make sure it works
  CustomizableUI.addWidgetToArea(
    "fullscreen-button",
    CustomizableUI.AREA_NAVBAR
  );
  registerCleanupFunction(() => CustomizableUI.reset());

  let inFullScreen = await toggle_fs();
  ok(inFullScreen, "Full screen should be ON");
  await check_states(inFullScreen);

  inFullScreen = await toggle_fs();
  is(inFullScreen, false, "Full screen should be OFF");
  await check_states(inFullScreen);
});
