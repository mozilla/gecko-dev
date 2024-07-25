/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PlacesTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PlacesTestUtils.sys.mjs"
);

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
const dates = [today, yesterday];

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

async function showHistorySidebar() {
  const { SidebarController } = win;
  if (SidebarController.currentID !== "viewHistorySidebar") {
    await SidebarController.show("viewHistorySidebar");
  }
  const { contentDocument, contentWindow } = SidebarController.browser;
  const component = contentDocument.querySelector("sidebar-history");
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
});

add_task(async function test_history_keyboard_navigation() {
  const {
    component: { cards, lists },
    contentWindow,
  } = await showHistorySidebar();

  // TODO: (Bug 1908742) Cards should be expanded already, this shouldn't be necessary.
  await TestUtils.waitForTick();
  for (const card of cards) {
    card.toggleDetails(true);
    await card.updateComplete;
  }
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
});

add_task(async function test_history_hover_buttons() {
  const {
    component: { cards, lists },
    contentWindow,
  } = await showHistorySidebar();

  // TODO: (Bug 1908742) Cards should be expanded already, this shouldn't be necessary.
  await TestUtils.waitForTick();
  for (const card of cards) {
    card.toggleDetails(true);
    await card.updateComplete;
  }
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
});

add_task(async function test_history_empty_state() {
  const { component } = await showHistorySidebar();
  info("Clear all history.");
  await PlacesUtils.history.clear();
  const emptyState = await TestUtils.waitForCondition(
    () => component.emptyState
  );
  ok(BrowserTestUtils.isVisible(emptyState), "Empty state is displayed.");
});
