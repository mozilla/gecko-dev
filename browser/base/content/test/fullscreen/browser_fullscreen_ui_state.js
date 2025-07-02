/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function click_hamburger_fs_toggle() {
  // show panel
  let menuShown = BrowserTestUtils.waitForEvent(PanelUI.panel, "ViewShown");
  PanelUI.show();
  await menuShown;

  // click button
  let hamburgerMenuButton = window.document.getElementById(
    "appMenu-fullscreen-button2"
  );

  // clicking the button might hide the menu
  let menuHidden = BrowserTestUtils.waitForEvent(PanelUI.panel, "popuphidden");

  hamburgerMenuButton.click();

  // try to hide panel explicitly
  PanelUI.hide();
  await menuHidden;
}

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
  // We add the custom button to the toolbar to make sure it works
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

add_task(async function test_f11_fullscreen() {
  const fullScreenEntered = BrowserTestUtils.waitForEvent(window, "fullscreen");
  info("Pressing F11 to enter fullscreen");
  EventUtils.synthesizeKey("KEY_F11", {});
  await fullScreenEntered;
  ok(window.fullScreen, "Full screen should be ON");

  const fullScreenExited = BrowserTestUtils.waitForEvent(window, "fullscreen");
  info("Pressing F11 to exit fullscreen");
  EventUtils.synthesizeKey("KEY_F11", {});
  await fullScreenExited;
  ok(!window.fullScreen, "Full screen should be OFF");
});

add_task(async function test_mac_fullscreen_shortcut() {
  if (AppConstants.platform !== "macosx") {
    info("Skipping test for MacOS shortcut on non-Mac");
    return;
  }

  const fullScreenEntered = BrowserTestUtils.waitForEvent(window, "fullscreen");
  info("Pressing Ctrl+Cmd+F to enter fullscreen");
  EventUtils.synthesizeKey("f", { ctrlKey: true, metaKey: true }, window);
  await fullScreenEntered;
  ok(window.fullScreen, "Full screen should be ON");

  const fullScreenExited = BrowserTestUtils.waitForEvent(window, "fullscreen");
  info("Pressing Ctrl+Cmd+F to enter fullscreen");
  EventUtils.synthesizeKey("f", { ctrlKey: true, metaKey: true }, window);
  await fullScreenExited;
  ok(!window.fullScreen, "Full screen should be OFF");
});

add_task(async function test_menubar_click() {
  let fsEnable;
  let fsDisable;
  if (AppConstants.platform === "macosx") {
    fsEnable = document.getElementById("enterFullScreenItem");
    fsDisable = document.getElementById("exitFullScreenItem");
  } else {
    fsEnable = document.getElementById("fullScreenItem");
    fsDisable = fsEnable;
  }

  const fullScreenEntered = BrowserTestUtils.waitForEvent(window, "fullscreen");
  info("Pressing Enter Full Screen");
  fsEnable.click();
  await fullScreenEntered;
  ok(window.fullScreen, "Full screen should be ON");

  const fullScreenExited = BrowserTestUtils.waitForEvent(window, "fullscreen");
  fsDisable.click();
  await fullScreenExited;
  ok(!window.fullScreen, "Full screen should be OFF");
});

add_task(async function test_hamburger_menu_click() {
  const fullScreenEntered = BrowserTestUtils.waitForEvent(window, "fullscreen");
  info("Pressing little double arrow icon");
  await click_hamburger_fs_toggle();
  await fullScreenEntered;
  ok(window.fullScreen, "Full screen should be ON");

  const fullScreenExited = BrowserTestUtils.waitForEvent(window, "fullscreen");
  info("Pressing little double arrow icon");
  await click_hamburger_fs_toggle();
  await fullScreenExited;
  ok(!window.fullScreen, "Full screen should be OFF");
});

add_task(async function test_custom_menu_icon_click() {
  // We add the custom button to the toolbar to make sure it works
  CustomizableUI.addWidgetToArea(
    "fullscreen-button",
    CustomizableUI.AREA_NAVBAR
  );

  registerCleanupFunction(() => CustomizableUI.reset());
  const toolbarButton = document.getElementById("fullscreen-button");
  const fullScreenEntered = BrowserTestUtils.waitForEvent(window, "fullscreen");
  info("Pressing custom double arrow icon");
  toolbarButton.click();
  await fullScreenEntered;
  ok(window.fullScreen, "Full screen should be ON");

  const fullScreenExited = BrowserTestUtils.waitForEvent(window, "fullscreen");
  info("Pressing custom double arrow icon");
  toolbarButton.click();
  await fullScreenExited;
  ok(!window.fullScreen, "Full screen should be OFF");
});
