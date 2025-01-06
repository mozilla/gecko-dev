"use strict";

add_setup(() =>
  SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.groups.enabled", true]],
  })
);

/**
 * Presses Cmd + arrow key/Ctrl + arrow key in order to move the keyboard
 * focused item in the tab strip left or right
 *
 * @param {MozTabbrowserTab|MozTextLabel} element
 * @param {"KEY_ArrowUp"|"KEY_ArrowDown"|"KEY_ArrowLeft"|"KEY_ArrowRight"} keyName
 * @returns {Promise<true>}
 */
function synthesizeKeyToChangeKeyboardFocus(element, keyName) {
  let focused = TestUtils.waitForCondition(() => {
    return element.classList.contains("tablist-keyboard-focus");
  }, "Waiting for element to get keyboard focus");
  EventUtils.synthesizeKey(keyName, { accelKey: true });
  return focused;
}

/**
 * Presses an arrow key in order to change the active tab or to change the
 * keyboard-focused item to a tab group label to the left or right.
 *
 * @param {MozTabbrowserTab|MozTextLabel} element
 * @param {"KEY_ArrowUp"|"KEY_ArrowDown"|"KEY_ArrowLeft"|"KEY_ArrowRight"} keyName
 * @returns {Promise<true>}
 */
function synthesizeKeyForKeyboardMovement(element, keyName) {
  let focused = TestUtils.waitForCondition(() => {
    return (
      element.classList.contains("tablist-keyboard-focus") ||
      document.activeElement == element
    );
  }, "Waiting for element to become active tab and/or get keyboard focus");
  EventUtils.synthesizeKey(keyName);
  return focused;
}

add_task(async function test_TabGroupKeyboardFocus() {
  const tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  const tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  const tab3 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  const tab4 = BrowserTestUtils.addTab(gBrowser, "about:blank");

  const tabGroup = gBrowser.addTabGroup([tab2, tab3], { insertBefore: tab2 });

  await BrowserTestUtils.switchTab(gBrowser, tab2);

  // The user normally needs to hit Tab/Shift+Tab in order to cycle
  // focused elements until the active tab is focused, but the number
  // of focusable elements in the browser changes depending on the user's
  // UI customizations. This code is forcing the focus to the active tab
  // to avoid any dependencies on specific UI configuration.
  info("Move focus to the active tab");
  Services.focus.setFocus(tab2, Services.focus.FLAG_BYKEY);

  is(document.activeElement, tab2, "Tab2 should be focused");
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tab2,
    "Tab2 should be keyboard-focused as well"
  );

  await synthesizeKeyToChangeKeyboardFocus(
    tabGroup.labelElement,
    "KEY_ArrowLeft"
  );
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tabGroup.labelElement,
    "keyboard focus should move left from tab to tab group label"
  );

  await synthesizeKeyToChangeKeyboardFocus(tab1, "KEY_ArrowLeft");
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tab1,
    "keyboard focus should move left from tab group label to tab"
  );

  await synthesizeKeyToChangeKeyboardFocus(
    tabGroup.labelElement,
    "KEY_ArrowRight"
  );
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tabGroup.labelElement,
    "keyboard focus should move right from tab to tab group label"
  );

  await synthesizeKeyToChangeKeyboardFocus(tab1, "KEY_ArrowUp");
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tab1,
    "keyboard focus 'up' should move left from tab group label to tab with LTR GUI"
  );

  await synthesizeKeyToChangeKeyboardFocus(
    tabGroup.labelElement,
    "KEY_ArrowDown"
  );
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tabGroup.labelElement,
    "keyboard focus 'down' should move right from tab to tab group label with LTR GUI"
  );

  info(
    "Validate that the keyboard can control the expanded/collapsed state of the tab group"
  );
  Assert.ok(!tabGroup.collapsed, "Tab group should be expanded");
  await EventUtils.synthesizeKey("VK_SPACE");
  Assert.ok(tabGroup.collapsed, "Tab group should be collapsed");

  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tabGroup.labelElement,
    "keyboard focus should remain on tab group label after collapse"
  );
  is(
    gBrowser.selectedTab,
    tab4,
    "active tab should automatically change to the next visible tab if the active tab is in a tab group that gets collapsed"
  );

  await EventUtils.synthesizeKey("KEY_Enter");
  Assert.ok(!tabGroup.collapsed, "Tab group should be expanded");
  await EventUtils.synthesizeKey("KEY_Enter");
  Assert.ok(tabGroup.collapsed, "Tab group should be collapsed once again");

  info("Validate that keyboard focus skips over tabs in collapsed tab groups");
  await synthesizeKeyToChangeKeyboardFocus(tab4, "KEY_ArrowRight");
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tab4,
    "keyboard focus should move right collapsed tab group label to the first tab to the right of the tab group"
  );

  await removeTabGroup(tabGroup);
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab4);
});

add_task(async function test_TabGroupKeyboardMovement() {
  const tab1 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  const tab2 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  const tab3 = BrowserTestUtils.addTab(gBrowser, "about:blank");
  const tab4 = BrowserTestUtils.addTab(gBrowser, "about:blank");

  const tabGroup = gBrowser.addTabGroup([tab2, tab3], { insertBefore: tab2 });

  await BrowserTestUtils.switchTab(gBrowser, tab2);

  // The user normally needs to hit Tab/Shift+Tab in order to cycle
  // focused elements until the active tab is focused, but the number
  // of focusable elements in the browser changes depending on the user's
  // UI customizations. This code is forcing the focus to the active tab
  // to avoid any dependencies on specific UI configuration.
  info("Move focus to the active tab");
  Services.focus.setFocus(tab2, Services.focus.FLAG_BYKEY);

  is(document.activeElement, tab2, "Tab2 should be focused");
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tab2,
    "Tab2 should be keyboard-focused as well"
  );

  await synthesizeKeyForKeyboardMovement(
    tabGroup.labelElement,
    "KEY_ArrowLeft"
  );
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tabGroup.labelElement,
    "keyboard focus should move left from tab to tab group label"
  );

  await synthesizeKeyForKeyboardMovement(tab1, "KEY_ArrowLeft");
  is(document.activeElement, tab1, "Tab1 should be focused");
  is(gBrowser.selectedTab, tab1, "Tab1 should be the active tab");
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tab1,
    "keyboard focus should move left from tab group label to tab"
  );

  await synthesizeKeyForKeyboardMovement(
    tabGroup.labelElement,
    "KEY_ArrowRight"
  );
  is(gBrowser.selectedTab, tab1, "Tab1 should still be the active tab");
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tabGroup.labelElement,
    "keyboard focus should move right from tab to tab group label"
  );

  await synthesizeKeyForKeyboardMovement(tab1, "KEY_ArrowUp");
  is(gBrowser.selectedTab, tab1, "Tab1 should still be the active tab");
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tab1,
    "keyboard focus 'up' should move left from tab group label to tab with LTR GUI"
  );

  await synthesizeKeyForKeyboardMovement(
    tabGroup.labelElement,
    "KEY_ArrowDown"
  );
  is(gBrowser.selectedTab, tab1, "Tab1 should still be the active tab");
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tabGroup.labelElement,
    "keyboard focus 'down' should move right from tab to tab group label with LTR GUI"
  );

  info(
    "Validate that the keyboard can control the expanded/collapsed state of the tab group"
  );
  Assert.ok(!tabGroup.collapsed, "Tab group should be expanded");
  await EventUtils.synthesizeKey("VK_SPACE");
  Assert.ok(tabGroup.collapsed, "Tab group should be collapsed");

  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tabGroup.labelElement,
    "keyboard focus should remain on tab group label after collapse"
  );
  is(gBrowser.selectedTab, tab1, "active tab should remain tab1");

  info(
    "Validate that the active tab selection skips over tabs in collapsed tab groups"
  );
  await synthesizeKeyForKeyboardMovement(tab4, "KEY_ArrowRight");
  is(gBrowser.selectedTab, tab4, "tab4 should become the active tab");
  is(
    gBrowser.tabContainer.ariaFocusedItem,
    tab4,
    "selected tab should be keyboard-focused as well"
  );

  await removeTabGroup(tabGroup);
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab4);
});
