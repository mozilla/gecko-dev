/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PlacesTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PlacesTestUtils.sys.mjs"
);

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  HistoryController: "resource:///modules/HistoryController.sys.mjs",
});

const URLs = [
  "http://mochi.test:8888/browser/",
  "https://www.example.com/",
  "https://example.net/",
  "https://example.org/",
];

const today = new Date();
const yesterday = new Date(
  today.getFullYear(),
  today.getMonth(),
  today.getDate() - 1
);

// Get date for the second-last day of the previous month.
// (Do not use the last day, since that could be the same as yesterday's date.)
const lastMonth = new Date(today.getFullYear(), today.getMonth(), -2);

const dates = [today, yesterday, lastMonth];

let win;

add_setup(async () => {
  await PlacesUtils.history.clear();
  const pageInfos = URLs.flatMap((url, i) =>
    dates.map(date => ({
      url,
      title: `Example Domain ${i}`,
      visits: [{ date }],
    }))
  );
  await PlacesUtils.history.insertMany(pageInfos);
  win = await BrowserTestUtils.openNewBrowserWindow();
});

registerCleanupFunction(async () => {
  await PlacesUtils.history.clear();
  await BrowserTestUtils.closeWindow(win);
});

async function showHistorySidebar({ waitForPendingHistory = true } = {}) {
  const { SidebarController } = win;
  if (SidebarController.currentID !== "viewHistorySidebar") {
    await SidebarController.show("viewHistorySidebar");
  }
  const { contentDocument, contentWindow } = SidebarController.browser;
  const component = contentDocument.querySelector("sidebar-history");
  if (waitForPendingHistory) {
    await BrowserTestUtils.waitForCondition(
      () => !component.controller.isHistoryPending
    );
  }
  await component.updateComplete;
  return { component, contentWindow };
}

async function waitForPageLoadTask(pageLoadTask, expectedUrl) {
  const promiseTabOpen = BrowserTestUtils.waitForEvent(
    win.gBrowser.tabContainer,
    "TabOpen"
  );
  await pageLoadTask();
  await promiseTabOpen;
  await TestUtils.waitForCondition(
    () => win.gBrowser.currentURI.spec === expectedUrl,
    `Navigated to ${expectedUrl}.`
  );
}

add_task(async function test_history_cards_created() {
  const {
    component: { lists },
  } = await showHistorySidebar();
  Assert.equal(lists.length, dates.length, "There is a card for each day.");
  for (const list of lists) {
    Assert.equal(
      list.tabItems.length,
      URLs.length,
      "Card shows the correct number of visits."
    );
  }
  win.SidebarController.hide();
});

add_task(async function test_history_searchbox_focus() {
  const { component } = await showHistorySidebar();
  const { searchTextbox } = component;

  ok(component.shadowRoot.activeElement, "check activeElement is present");
  Assert.equal(
    component.shadowRoot.activeElement,
    searchTextbox,
    "Check search box is focused"
  );
  win.SidebarController.hide();
});

add_task(async function test_history_searchbox_focused_with_history_pending() {
  const sandbox = sinon.createSandbox();

  // This stubs any new instance created so that isHistoryPending getter always
  // returns true to simulate waiting for history to load.
  sandbox
    .stub(lazy.HistoryController.prototype, "isHistoryPending")
    .value(true);

  const { SidebarController } = win;

  // Show the new history sidebar but don't wait for pendingHistory as this will timeout
  // since the check isHistoryPending will always return true.
  const { component } = await showHistorySidebar({
    waitForPendingHistory: false,
  });
  const { searchTextbox } = component;

  ok(component.shadowRoot.activeElement, "check activeElement is present");
  Assert.equal(
    component.shadowRoot.activeElement,
    searchTextbox,
    "Check search box is focused"
  );

  // Clean-up by hiding the sidebar, because the instance associated with this
  // History Sidebar remains overidden and the sandbox.restore() only affects
  // new instances that are created.
  SidebarController.hide();
  sandbox.restore();
});

add_task(async function test_history_search() {
  const { component, contentWindow } = await showHistorySidebar();
  const { searchTextbox } = component;

  info("Input a search query.");
  EventUtils.synthesizeMouseAtCenter(searchTextbox, {}, contentWindow);
  EventUtils.sendString("Example Domain 1", contentWindow);
  await BrowserTestUtils.waitForMutationCondition(
    component.shadowRoot,
    { childList: true, subtree: true },
    () =>
      component.lists.length === 1 &&
      component.shadowRoot.querySelector(
        "moz-card[data-l10n-id=sidebar-search-results-header]"
      )
  );
  await TestUtils.waitForCondition(() => {
    const { rowEls } = component.lists[0];
    return rowEls.length === 1 && rowEls[0].mainEl.href === URLs[1];
  }, "There is one matching search result.");

  info("Input a bogus search query.");
  EventUtils.synthesizeMouseAtCenter(searchTextbox, {}, contentWindow);
  EventUtils.sendString("Bogus Query", contentWindow);
  await TestUtils.waitForCondition(() => {
    const tabList = component.lists[0];
    return tabList?.emptyState;
  }, "There are no matching search results.");

  info("Clear the search query.");
  EventUtils.synthesizeMouseAtCenter(
    searchTextbox.clearButton,
    {},
    contentWindow
  );
  await TestUtils.waitForCondition(
    () => !component.lists[0].emptyState,
    "The original cards are restored."
  );
  win.SidebarController.hide();
});

add_task(async function test_history_sort() {
  const { component, contentWindow } = await showHistorySidebar();
  const { menuButton } = component;
  const menu = component._menu;
  const sortByDateButton = component._menuSortByDate;
  const sortBySiteButton = component._menuSortBySite;

  info("Sort history by site.");
  let promiseMenuShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(menuButton, {}, contentWindow);
  await promiseMenuShown;
  menu.activateItem(sortBySiteButton);
  await TestUtils.waitForCondition(
    () => component.lists.length === URLs.length,
    "There is a card for each site."
  );
  Assert.equal(
    sortBySiteButton.getAttribute("checked"),
    "true",
    "Sort by site is checked."
  );
  for (const card of component.cards) {
    Assert.equal(card.expanded, true, "All cards are expanded.");
  }

  info("Sort history by date.");
  promiseMenuShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(menuButton, {}, contentWindow);
  await promiseMenuShown;
  menu.activateItem(sortByDateButton);
  await TestUtils.waitForCondition(
    () => component.lists.length === dates.length,
    "There is a card for each date."
  );
  Assert.equal(
    sortByDateButton.getAttribute("checked"),
    "true",
    "Sort by date is checked."
  );
  for (const [i, card] of component.cards.entries()) {
    Assert.equal(
      card.expanded,
      i === 0 || i === 1,
      "The cards for Today and Yesterday are expanded."
    );
  }
  win.SidebarController.hide();
});

add_task(async function test_history_keyboard_navigation() {
  const {
    component: { lists },
    contentWindow,
  } = await showHistorySidebar();

  const rows = await TestUtils.waitForCondition(
    () => lists[0].rowEls.length === URLs.length && lists[0].rowEls,
    "History rows are shown."
  );
  rows[0].focus();

  info("Focus the next row.");
  let focused = BrowserTestUtils.waitForEvent(rows[1], "focus", contentWindow);
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, contentWindow);
  await focused;

  info("Focus the previous row.");
  focused = BrowserTestUtils.waitForEvent(rows[0], "focus", contentWindow);
  EventUtils.synthesizeKey("KEY_ArrowUp", {}, contentWindow);
  await focused;

  info("Open the focused link.");
  await waitForPageLoadTask(
    () => EventUtils.synthesizeKey("KEY_Enter", {}, contentWindow),
    URLs[0]
  );
  win.SidebarController.hide();
});

add_task(async function test_history_hover_buttons() {
  const {
    component: { lists },
    contentWindow,
  } = await showHistorySidebar();

  const rows = await TestUtils.waitForCondition(
    () => lists[0].rowEls.length === URLs.length && lists[0].rowEls,
    "History rows are shown."
  );

  info("Open the first link.");
  await waitForPageLoadTask(
    () => EventUtils.synthesizeMouseAtCenter(rows[0].mainEl, {}, contentWindow),
    URLs[0]
  );

  info("Remove the first entry.");
  const promiseRemoved = PlacesTestUtils.waitForNotification("page-removed");
  EventUtils.synthesizeMouseAtCenter(
    rows[0].secondaryButtonEl,
    {},
    contentWindow
  );
  await promiseRemoved;
  await TestUtils.waitForCondition(
    () => lists[0].rowEls.length === URLs.length - 1,
    "The removed entry should no longer be visible."
  );
  win.SidebarController.hide();
});

add_task(async function test_history_context_menu() {
  const {
    component: { lists },
  } = await showHistorySidebar();
  const contextMenu = win.SidebarController.currentContextMenu;

  let rows = await TestUtils.waitForCondition(
    () => lists[0].rowEls.length && lists[0].rowEls,
    "History rows are shown."
  );

  function getItem(item) {
    return win.document.getElementById("sidebar-history-context-" + item);
  }

  info("Delete from history.");
  const promiseRemoved = PlacesTestUtils.waitForNotification("page-removed");
  await openAndWaitForContextMenu(contextMenu, rows[0].mainEl, () =>
    contextMenu.activateItem(getItem("delete-page"))
  );
  await promiseRemoved;
  await TestUtils.waitForCondition(
    () => lists[0].rowEls.length === URLs.length - 2,
    "The removed entry should no longer be visible."
  );

  rows = lists[0].rowEls;
  const { url } = rows[0];

  info("Open link in a new window.");
  let promiseWin = BrowserTestUtils.waitForNewWindow({ url });
  await openAndWaitForContextMenu(contextMenu, rows[0].mainEl, () =>
    contextMenu.activateItem(getItem("open-in-window"))
  );
  await BrowserTestUtils.closeWindow(await promiseWin);

  info("Open link in a new private window.");
  promiseWin = BrowserTestUtils.waitForNewWindow({ url });
  await openAndWaitForContextMenu(contextMenu, rows[0].mainEl, () =>
    contextMenu.activateItem(getItem("open-in-private-window"))
  );
  const privateWin = await promiseWin;
  ok(
    PrivateBrowsingUtils.isWindowPrivate(privateWin),
    "The new window is in private browsing mode."
  );
  await BrowserTestUtils.closeWindow(privateWin);

  info(`Copy link from: ${rows[0].mainEl.href}`);
  await openAndWaitForContextMenu(contextMenu, rows[0].mainEl, () =>
    contextMenu.activateItem(getItem("copy-link"))
  );
  const copiedUrl = SpecialPowers.getClipboardData("text/plain");
  is(copiedUrl, url, "The copied URL is correct.");
  win.SidebarController.hide();
});

add_task(async function test_history_empty_state() {
  const { component } = await showHistorySidebar();
  info("Clear all history.");
  await PlacesUtils.history.clear();
  const emptyState = await TestUtils.waitForCondition(
    () => component.emptyState
  );
  ok(BrowserTestUtils.isVisible(emptyState), "Empty state is displayed.");
  win.SidebarController.hide();
});
