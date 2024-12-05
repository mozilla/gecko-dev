/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

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
  // This is helpful to avoid some weird race conditions in the test, specifically
  // the assertion that !this.blankTab in AsyncTabSwitcher when adding a new tab.
  //await promiseTabLoadEvent(gBrowser.selectedTab, "http://mochi.test:8888/#originalTab");
  let tabs = [];
  for (let i = 0; i < numberOfTabs; i++) {
    tabs.push(await addTab(`http://mochi.test:8888/#${i}`));
  }
  return tabs;
}

function injectBeforeUnload(browser) {
  return ContentTask.spawn(browser, null, async function () {
    content.window.addEventListener(
      "beforeunload",
      function (event) {
        var str = "Leaving?";
        event.returnValue = str;
        return str;
      },
      true
    );
  });
}

// Wait for onbeforeunload dialog, and dismiss it immediately.
function awaitAndCloseBeforeUnloadDialog(browser, doStayOnPage) {
  return PromptTestUtils.handleNextPrompt(
    browser,
    { modalType: Services.prompt.MODAL_TYPE_CONTENT, promptType: "confirmEx" },
    { buttonNumClick: doStayOnPage ? 1 : 0 }
  );
}

/* global messageManager */

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

add_task(async function test_unload_selected_and_allow() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.unloadTabInContextMenu", true],
      ["dom.require_user_interaction_for_beforeunload", false],
    ],
  });

  let [tab1, tab2] = await addBrowserTabs(2);
  await injectBeforeUnload(tab1.linkedBrowser);

  let menuItemUnload = document.getElementById("context_unloadTab");
  let tab1Promise = awaitAndCloseBeforeUnloadDialog(tab1.linkedBrowser, false);
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  {
    let menu = await openTabMenuFor(tab1);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }
  await tab1Promise;
  await TestUtils.waitForCondition(
    () => !tab1.linkedPanel,
    "Wait for Tab1 to be unloaded"
  );
  is(gBrowser.selectedTab, tab2, "Should select another loaded tab");

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
});

add_task(async function test_unload_selected_and_block() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.unloadTabInContextMenu", true],
      ["dom.require_user_interaction_for_beforeunload", false],
    ],
  });

  let [tab1, tab2] = await addBrowserTabs(2);
  await injectBeforeUnload(tab1.linkedBrowser);

  let menuItemUnload = document.getElementById("context_unloadTab");
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  {
    let menu = await openTabMenuFor(tab1);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }
  await awaitAndCloseBeforeUnloadDialog(tab1.linkedBrowser, true);
  ok(tab1.linkedPanel, "tab1 should still be loaded");
  is(gBrowser.selectedTab, tab1, "tab1 should still be selected");

  await BrowserTestUtils.removeTab(tab2);
  // Make sure this tab actually closes
  let beforeUnloadPromise = awaitAndCloseBeforeUnloadDialog(
    tab1.linkedBrowser,
    false
  );
  await BrowserTestUtils.removeTab(tab1);
  await beforeUnloadPromise;
});

add_task(async function test_unload_multiple_and_allow() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.unloadTabInContextMenu", true],
      ["dom.require_user_interaction_for_beforeunload", false],
    ],
  });

  let [tab1, tab2, tab3] = await addBrowserTabs(3);
  await injectBeforeUnload(tab2.linkedBrowser);
  await injectBeforeUnload(tab3.linkedBrowser);

  let menuItemUnload = document.getElementById("context_unloadTab");
  await BrowserTestUtils.switchTab(gBrowser, tab2);
  await triggerClickOn(tab3, { ctrlKey: true });
  let tab2Promise = awaitAndCloseBeforeUnloadDialog(tab2.linkedBrowser, false);
  let tab3Promise = awaitAndCloseBeforeUnloadDialog(tab3.linkedBrowser, false);
  {
    let menu = await openTabMenuFor(tab2);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }
  await tab2Promise;
  await tab3Promise;
  await TestUtils.waitForCondition(
    () => !tab2.linkedPanel && !tab3.linkedPanel,
    "Wait for Tab2 and Tab3 to be unloaded"
  );
  is(gBrowser.selectedTab, tab1, "Should select another loaded tab");

  // Make sure tabs actually close
  await BrowserTestUtils.removeTab(tab3);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_unload_selected_and_block() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.unloadTabInContextMenu", true],
      ["dom.require_user_interaction_for_beforeunload", false],
    ],
  });

  let [tab1, tab2] = await addBrowserTabs(2);
  await injectBeforeUnload(tab1.linkedBrowser);

  let menuItemUnload = document.getElementById("context_unloadTab");
  await BrowserTestUtils.switchTab(gBrowser, tab1);
  {
    let menu = await openTabMenuFor(tab1);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }
  await awaitAndCloseBeforeUnloadDialog(tab1.linkedBrowser, true);
  ok(tab1.linkedPanel, "tab1 should still be loaded");
  is(gBrowser.selectedTab, tab1, "tab1 should still be selected");

  await BrowserTestUtils.removeTab(tab2);
  // Make sure this tab actually closes
  let beforeUnloadPromise = awaitAndCloseBeforeUnloadDialog(
    tab1.linkedBrowser,
    false
  );
  await BrowserTestUtils.removeTab(tab1);
  await beforeUnloadPromise;
});

add_task(async function test_unload_multiple_and_block() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.unloadTabInContextMenu", true],
      ["dom.require_user_interaction_for_beforeunload", false],
    ],
  });

  let [tab1, tab2, tab3] = await addBrowserTabs(3);
  await injectBeforeUnload(tab3.linkedBrowser);

  let menuItemUnload = document.getElementById("context_unloadTab");
  await BrowserTestUtils.switchTab(gBrowser, tab2);
  await triggerClickOn(tab3, { ctrlKey: true });
  let tab3Promise = awaitAndCloseBeforeUnloadDialog(tab3.linkedBrowser, true);
  {
    let menu = await openTabMenuFor(tab2);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }
  await tab3Promise;

  ok(tab1.linkedPanel, "tab1 should still be loaded");
  ok(tab2.linkedPanel, "tab2 should still be loaded");
  ok(tab3.linkedPanel, "tab3 should still be loaded");

  // Make sure tabs actually close
  tab3Promise = awaitAndCloseBeforeUnloadDialog(tab3.linkedBrowser, false);
  await BrowserTestUtils.removeTab(tab3);
  await tab3Promise;
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_unload_multiple_and_allow() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.unloadTabInContextMenu", true],
      ["dom.require_user_interaction_for_beforeunload", false],
    ],
  });

  let [tab1, tab2, tab3] = await addBrowserTabs(3);
  await injectBeforeUnload(tab3.linkedBrowser);

  let menuItemUnload = document.getElementById("context_unloadTab");
  await BrowserTestUtils.switchTab(gBrowser, tab2);
  await triggerClickOn(tab3, { ctrlKey: true });
  let tab3Promise = awaitAndCloseBeforeUnloadDialog(tab3.linkedBrowser, false);
  {
    let menu = await openTabMenuFor(tab2);
    let menuHiddenPromise = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
    menu.activateItem(menuItemUnload);
    await menuHiddenPromise;
  }
  await tab3Promise;
  await TestUtils.waitForCondition(
    () => !tab2.linkedPanel && !tab3.linkedPanel,
    "Wait for Tab2 and Tab3 to be unloaded"
  );

  ok(tab1.linkedPanel, "tab1 should still be loaded");
  is(gBrowser.selectedTab, tab1, "Should select another loaded tab");

  await BrowserTestUtils.removeTab(tab3);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab1);
});

add_task(async function test_cleanup() {
  await BrowserTestUtils.removeTab(FirefoxViewHandler.tab);
});
