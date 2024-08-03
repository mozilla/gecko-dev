/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that panel switcher adds conditional sidebars
 */
add_task(async function test_switcher_conditional() {
  const switcherPanel = document.getElementById("sidebarMenu-popup");
  const origCount = switcherPanel.childElementCount;

  await SpecialPowers.pushPrefEnv({ set: [["browser.ml.chat.enabled", true]] });
  is(switcherPanel.childElementCount, origCount + 1, "Added menu item");
});

/**
 * Check that activating the switcher while already open stays open
 */
add_task(async function test_switcher_twice() {
  ok(!SidebarController.isOpen, "Starts closed");

  const item = document.getElementById("sidebar-switcher-genai-chat");
  item.click();
  ok(SidebarController.isOpen, "Opens on first click");

  item.click();
  ok(SidebarController.isOpen, "Stays open on second click");

  SidebarController.hide();
}).skip(); // bug 1896421
