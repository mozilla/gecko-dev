/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Opens the context menu on a tab and waits for it to be shown.
 *
 * @param tab The tab to open a context menu on
 * @returns {Promise} Promise object with the menu.
 */
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

/**
 * Adds new tabs to `gBrowser`.
 *
 * @param {number} numberOfTabs The number of new tabs to add.
 * @returns {Promise<Array>} Promise object containing the new tabs that were added.
 */
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

  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.unloadTabInContextMenu", true]],
  });
});

/**
 * Checks various common properties of a tabExplicitUnload event.
 *
 * @param e The telemetry event to check
 */
function checkEventCommon(e) {
  Assert.equal(e.category, "browser.engagement", "correct category");
  Assert.equal(e.name, "tab_explicit_unload", "correct name");
  // Since we're unloading small pages here, the memory may not change much or might even
  // go up a smidge.
  let memoryBefore = parseInt(e.extra.memory_before, 10);
  let memoryAfter = parseInt(e.extra.memory_after, 10);
  Assert.less(
    memoryAfter - memoryBefore,
    100000,
    `Memory should go down after unload (before: ${memoryBefore}, after ${memoryAfter})`
  );
  Assert.greaterOrEqual(
    parseInt(e.extra.time_to_unload_in_ms, 10),
    0,
    "time_to_unload should be >= 0"
  );
  Assert.less(
    parseInt(e.extra.time_to_unload_in_ms, 10),
    10000,
    "time_to_unload should be within reason"
  );
}

/**
 * Gets a promise that will be fulfilled when the tab is unloaded.
 *
 * @param tab The tab that will be unloaded
 * @returns {Promise} A promise that will be fulfilled when
 *                    the tab is unloaded.
 */
function getWaitForUnloadedPromise(tab) {
  return BrowserTestUtils.waitForEvent(tab, "TabBrowserDiscarded");
}

/**
 * Waits for a tabExplicitUnload telemetry event to occur.
 *
 * @returns {Promise} A promise that will be fulfilled when the
 *                    telemetry event occurs.
 */
async function waitForTelemetryEvent() {
  await TestUtils.waitForCondition(() => {
    let events = Glean.browserEngagement.tabExplicitUnload.testGetValue();
    if (!events) {
      return false;
    }
    return !!events.length;
  });
}

/**
 * Unload the currently selected tab and one other and ensure
 * the telemetry is correct.
 */
add_task(async function test_unload_selected_and_one_other_tab() {
  let [tab1, tab2, tab3] = await addBrowserTabs(3);

  let menuItemUnload = document.getElementById("context_unloadTab");

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await triggerClickOn(tab2, { ctrlKey: true });

  Services.fog.testResetFOG();
  updateTabContextMenu(tab1);
  let tab1UnloadedPromise = getWaitForUnloadedPromise(tab1);
  let tab2UnloadedPromise = getWaitForUnloadedPromise(tab2);
  {
    let menu = await openTabMenuFor(tab1);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }

  await Promise.all([tab1UnloadedPromise, tab2UnloadedPromise]);

  await waitForTelemetryEvent();
  let unloadTelemetry =
    Glean.browserEngagement.tabExplicitUnload.testGetValue();
  Assert.equal(
    unloadTelemetry.length,
    1,
    "should get exactly one telemetry event"
  );
  let e = unloadTelemetry[0];
  checkEventCommon(e);
  Assert.equal(e.extra.unload_selected_tab, "true", "did unload selected tab");
  Assert.equal(e.extra.all_tabs_unloaded, "false", "did not unload everything");
  Assert.equal(e.extra.tabs_unloaded, "2", "correct number of tabs unloaded");

  Services.fog.testResetFOG();
  await BrowserTestUtils.removeTab(tab3);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab1);
});

/**
 * Unload one unselected tab and ensure the telemetry is correct.
 */
add_task(async function test_unload_one_unselected_tab() {
  let [tab1, tab2, tab3] = await addBrowserTabs(3);

  let menuItemUnload = document.getElementById("context_unloadTab");
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  updateTabContextMenu(tab2);
  Services.fog.testResetFOG();
  let tab2UnloadedPromise = getWaitForUnloadedPromise(tab2);
  {
    let menu = await openTabMenuFor(tab2);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }
  await tab2UnloadedPromise;

  await waitForTelemetryEvent();
  let unloadTelemetry =
    Glean.browserEngagement.tabExplicitUnload.testGetValue();
  Assert.equal(
    unloadTelemetry.length,
    1,
    "should get exactly one telemetry event"
  );
  let e = unloadTelemetry[0];
  checkEventCommon(e);
  Assert.equal(
    e.extra.unload_selected_tab,
    "false",
    "did not unload selected tab"
  );
  Assert.equal(e.extra.all_tabs_unloaded, "false", "did not unload everything");
  Assert.equal(e.extra.tabs_unloaded, "1", "correct number of tabs unloaded");

  Services.fog.testResetFOG();
  await BrowserTestUtils.removeTab(tab3);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab1);
});

/**
 * Unload just the selected tab and ensure the telemetry is correct.
 */
add_task(async function test_unload_selected_tab() {
  let [tab1, tab2, tab3] = await addBrowserTabs(3);

  let menuItemUnload = document.getElementById("context_unloadTab");
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  Services.fog.testResetFOG();
  let tab1UnloadedPromise = getWaitForUnloadedPromise(tab1);
  {
    let menu = await openTabMenuFor(tab1);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }
  await tab1UnloadedPromise;

  await waitForTelemetryEvent();
  let unloadTelemetry =
    Glean.browserEngagement.tabExplicitUnload.testGetValue();
  Assert.equal(
    unloadTelemetry.length,
    1,
    "should get exactly one telemetry event"
  );
  let e = unloadTelemetry[0];
  checkEventCommon(e);
  Assert.equal(e.extra.unload_selected_tab, "true", "did unload selected tab");
  Assert.equal(e.extra.all_tabs_unloaded, "false", "did not unload everything");
  Assert.equal(e.extra.tabs_unloaded, "1", "correct number of tabs unloaded");

  Services.fog.testResetFOG();
  await BrowserTestUtils.removeTab(tab3);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab1);
});

/**
 * Unload all tabs in the window and ensure
 * the telemetry is correct.
 */
add_task(async function test_unload_all_tabs() {
  let originalTab = gBrowser.selectedTab;
  let [tab1, tab2, tab3] = await addBrowserTabs(3);

  let menuItemUnload = document.getElementById("context_unloadTab");
  await triggerClickOn(tab1, { ctrlKey: true });
  await triggerClickOn(tab2, { ctrlKey: true });
  await triggerClickOn(tab3, { ctrlKey: true });
  updateTabContextMenu(originalTab);
  Services.fog.testResetFOG();
  let allTabsUnloadedPromises = [originalTab, tab1, tab2, tab3].map(
    getWaitForUnloadedPromise
  );
  {
    let menu = await openTabMenuFor(originalTab);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }

  await Promise.all(allTabsUnloadedPromises);

  await waitForTelemetryEvent();
  let unloadTelemetry =
    Glean.browserEngagement.tabExplicitUnload.testGetValue();
  Assert.equal(
    unloadTelemetry.length,
    1,
    "should get exactly one telemetry event"
  );
  let e = unloadTelemetry[0];
  checkEventCommon(e);
  Assert.equal(e.extra.unload_selected_tab, "true", "did unload selected tab");
  Assert.equal(e.extra.all_tabs_unloaded, "true", "did unload everything");
  Assert.equal(e.extra.tabs_unloaded, "4", "correct number of tabs unloaded");

  Services.fog.testResetFOG();
  await BrowserTestUtils.removeTab(tab3);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_cleanup() {
  await BrowserTestUtils.removeTab(FirefoxViewHandler.tab);
});
