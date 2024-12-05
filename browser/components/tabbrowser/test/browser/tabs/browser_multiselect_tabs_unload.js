/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

async function openTabMenuFor(tab) {
  let tabMenu = tab.ownerDocument.getElementById("tabContextMenu");

  let tabMenuShown = BrowserTestUtils.waitForPopupEvent(tabMenu, "shown");
  EventUtils.synthesizeMouseAtCenter(
    tab,
    { type: "contextmenu" },
    tab.ownerGlobal
  );
  await tabMenuShown;

  return tabMenu;
}

async function addBrowserTabs(numberOfTabs) {
  let tabs = [];
  for (let i = 0; i < numberOfTabs; i++) {
    tabs.push(await addTab(`http://mochi.test:8888/#${i}`));
  }
  return tabs;
}

add_setup(async function () {
  // This is helpful to avoid some weird race conditions in the test, specifically
  // the assertion that !this.blankTab in AsyncTabSwitcher when adding a new tab.
  await promiseTabLoadEvent(
    gBrowser.selectedTab,
    "http://mochi.test:8888/#originalTab"
  );
  let originalTab = gBrowser.selectedTab;
  // switch to Firefox View tab to initialize it
  FirefoxViewHandler.openTab();
  // switch back to the original tab since tests expect this
  await BrowserTestUtils.switchTab(gBrowser, originalTab);
});

add_task(async function test_unload_selected_and_one_other_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.unloadTabInContextMenu", true]],
  });

  let [tab1, tab2, tab3] = await addBrowserTabs(3);

  let menuItemUnload = document.getElementById("context_unloadTab");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab2, { ctrlKey: true });

  ok(tab1.multiselected, "Tab1 is multiselected");
  ok(tab2.multiselected, "Tab2 is multiselected");
  ok(!tab3.multiselected, "Tab3 is not multiselected");

  // Check the context menu with a multiselected tabs
  updateTabContextMenu(tab2);
  ok(!menuItemUnload.hidden, "Unload Tab is visible");
  is(
    JSON.parse(menuItemUnload.getAttribute("data-l10n-args")).tabCount,
    2,
    "showing 2 tabs on menu item"
  );

  // Check the context menu with a non-multiselected tab
  updateTabContextMenu(tab3);
  ok(!menuItemUnload.hidden, "Unload Tab is visible");
  is(
    JSON.parse(menuItemUnload.getAttribute("data-l10n-args")).tabCount,
    1,
    "showing 1 tabs on menu item"
  );

  {
    let menu = await openTabMenuFor(tab3);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }
  await TestUtils.waitForCondition(
    () => !tab3.linkedPanel,
    "Wait for Tab3 to be unloaded"
  );

  await BrowserTestUtils.switchTab(gBrowser, tab3);
  await TestUtils.waitForCondition(
    () => tab3.linkedPanel,
    "Wait for Tab3 to be loaded again"
  );

  await BrowserTestUtils.removeTab(tab3);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_unload_one_unselected_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.unloadTabInContextMenu", true]],
  });

  let [tab1, tab2, tab3] = await addBrowserTabs(3);

  let menuItemUnload = document.getElementById("context_unloadTab");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  updateTabContextMenu(tab2);
  ok(!menuItemUnload.hidden, "Unload Tab is visible");
  is(
    JSON.parse(menuItemUnload.getAttribute("data-l10n-args")).tabCount,
    1,
    "showing 1 tab on menu item"
  );
  {
    let menu = await openTabMenuFor(tab2);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }
  await TestUtils.waitForCondition(
    () => !tab2.linkedPanel,
    "Wait for Tab2 to be unloaded"
  );
  is(gBrowser.selectedTab, tab1, "Should stay on current tab");
  ok(tab1.linkedPanel, "Tab1 still loaded");
  ok(tab3.linkedPanel, "Tab3 still loaded");

  await BrowserTestUtils.removeTab(tab3);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_unload_selected_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.unloadTabInContextMenu", true]],
  });

  let [tab1, tab2, tab3] = await addBrowserTabs(3);

  let menuItemUnload = document.getElementById("context_unloadTab");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  updateTabContextMenu(tab1);
  ok(!menuItemUnload.hidden, "Unload Tab is visible");
  is(
    JSON.parse(menuItemUnload.getAttribute("data-l10n-args")).tabCount,
    1,
    "showing 1 tabs on menu item"
  );
  {
    let menu = await openTabMenuFor(tab1);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }
  await TestUtils.waitForCondition(
    () => !tab1.linkedPanel,
    "Wait for Tab1 to be unloaded"
  );
  is(gBrowser.selectedTab, tab2, "Should select another tab");
  ok(tab2.linkedPanel && tab3.linkedPanel, "Other tabs should be loaded");

  await BrowserTestUtils.removeTab(tab3);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_unload_all_tabs() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.unloadTabInContextMenu", true]],
  });

  let originalTab = gBrowser.selectedTab;
  let [tab1, tab2, tab3] = await addBrowserTabs(3);

  let menuItemUnload = document.getElementById("context_unloadTab");

  await triggerClickOn(tab1, { ctrlKey: true });
  await triggerClickOn(tab2, { ctrlKey: true });
  await triggerClickOn(tab3, { ctrlKey: true });
  updateTabContextMenu(originalTab);
  ok(!menuItemUnload.hidden, "Unload Tab is visible");
  is(
    JSON.parse(menuItemUnload.getAttribute("data-l10n-args")).tabCount,
    4,
    "showing 4 tabs on menu item"
  );
  {
    let menu = await openTabMenuFor(originalTab);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }
  await TestUtils.waitForCondition(
    () =>
      !originalTab.linkedPanel &&
      !tab1.linkedPanel &&
      !tab2.linkedPanel &&
      !tab3.linkedPanel,
    "Wait for all tabs to be unloaded"
  );
  is(
    gBrowser.selectedTab,
    FirefoxViewHandler.tab,
    "Should select Firefox View tab"
  );

  await BrowserTestUtils.removeTab(tab3);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_pref_off_does_not_show_unload_menu_item() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.unloadTabInContextMenu", false]],
  });

  let originalTab = gBrowser.selectedTab;
  let menuItemUnload = document.getElementById("context_unloadTab");
  updateTabContextMenu(originalTab);
  ok(menuItemUnload.hidden, "Unload Tab is hidden");
});

add_task(
  async function test_cannot_unload_tab_does_not_show_unload_menu_item() {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.tabs.unloadTabInContextMenu", true]],
    });

    let tab1 = await addTab("about:config");
    let menuItemUnload = document.getElementById("context_unloadTab");
    updateTabContextMenu(tab1);
    ok(menuItemUnload.hidden, "Unload Tab is hidden");

    await BrowserTestUtils.removeTab(tab1);
  }
);

add_task(async function test_cleanup() {
  await BrowserTestUtils.removeTab(FirefoxViewHandler.tab);
});
