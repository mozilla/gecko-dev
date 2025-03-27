/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PlacesTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PlacesTestUtils.sys.mjs"
);
// Tests the "Forget About This Site" button from the context menu
const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
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
  await BrowserTestUtils.browserLoaded(win.gBrowser, false, expectedUrl);
  info(`Navigated to ${expectedUrl}.`);
}

// TO DO - move below helpers into universal helper with Places Bug 1954843
/**
 * Fills a bookmarks dialog text field ensuring to cause expected edit events.
 *
 * @param {string} id
 *        id of the text field
 * @param {string} text
 *        text to fill in
 * @param {object} window
 *        dialog window
 * @param {boolean} [blur]
 *        whether to blur at the end.
 */
function fillBookmarkTextField(id, text, window, blur = true) {
  let elt = window.document.getElementById(id);
  elt.focus();
  elt.select();
  if (!text) {
    EventUtils.synthesizeKey("VK_DELETE", {}, window);
  } else {
    for (let c of text.split("")) {
      EventUtils.synthesizeKey(c, {}, window);
    }
  }
  if (blur) {
    elt.blur();
  }
}

/**
 * Executes a task after opening the bookmarks dialog, then cancels the dialog.
 *
 * @param {boolean} autoCancel
 *        whether to automatically cancel the dialog at the end of the task
 * @param {Function} openFn
 *        generator function causing the dialog to open
 * @param {Function} taskFn
 *        the task to execute once the dialog is open
 * @param {Function} closeFn
 *        A function to be used to wait for pending work when the dialog is
 *        closing. It is passed the dialog window handle and should return a promise.
 * @returns {string} guid
 *          Bookmark guid
 */
var withBookmarksDialog = async function (autoCancel, openFn, taskFn, closeFn) {
  let dialogUrl = "chrome://browser/content/places/bookmarkProperties.xhtml";
  let closed = false;
  // We can't show the in-window prompt for windows which don't have
  // gDialogBox, like the library (Places:Organizer) window.
  let hasDialogBox = !!Services.wm.getMostRecentWindow("").gDialogBox;
  let dialogPromise;
  if (hasDialogBox) {
    dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(null, dialogUrl, {
      isSubDialog: true,
    });
  } else {
    dialogPromise = BrowserTestUtils.domWindowOpenedAndLoaded(null, window => {
      return window.document.documentURI.startsWith(dialogUrl);
    }).then(window => {
      ok(
        window.location.href.startsWith(dialogUrl),
        "The bookmark properties dialog is open: " + window.location.href
      );
      // This is needed for the overlay.
      return SimpleTest.promiseFocus(window).then(() => window);
    });
  }
  let dialogClosePromise = dialogPromise.then(window => {
    if (!hasDialogBox) {
      return BrowserTestUtils.domWindowClosed(window);
    }
    let container = window.top.document.getElementById("window-modal-dialog");
    return BrowserTestUtils.waitForEvent(container, "close").then(() => {
      return BrowserTestUtils.waitForMutationCondition(
        container,
        { childList: true, attributes: true },
        () => !container.hasChildNodes() && !container.open
      );
    });
  });
  dialogClosePromise.then(() => {
    closed = true;
  });

  info("withBookmarksDialog: opening the dialog");
  // The dialog might be modal and could block our events loop, so executeSoon.
  executeSoon(openFn);

  info("withBookmarksDialog: waiting for the dialog");
  let dialogWin = await dialogPromise;

  // Ensure overlay is loaded
  info("waiting for the overlay to be loaded");
  await dialogWin.document.mozSubdialogReady;

  // Check the first input is focused.
  let doc = dialogWin.document;
  let elt = doc.querySelector('input:not([hidden="true"])');
  ok(elt, "There should be an input to focus.");

  if (elt) {
    info("waiting for focus on the first textfield");
    await TestUtils.waitForCondition(
      () => doc.activeElement == elt,
      "The first non collapsed input should have been focused"
    );
  }

  info("withBookmarksDialog: executing the task");

  let closePromise = () => Promise.resolve();
  if (closeFn) {
    closePromise = closeFn(dialogWin);
  }
  let guid;
  try {
    await taskFn(dialogWin);
  } finally {
    if (!closed && autoCancel) {
      info("withBookmarksDialog: canceling the dialog");
      doc.getElementById("bookmarkpropertiesdialog").cancelDialog();
      await closePromise;
    }
    guid = await PlacesUIUtils.lastBookmarkDialogDeferred.promise;
    // Give the dialog a little time to close itself.
    await dialogClosePromise;
  }
  return guid;
};

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
      ) &&
      component.lists[0]
  );
  await BrowserTestUtils.waitForCondition(() => {
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
  await BrowserTestUtils.waitForMutationCondition(
    component.shadowRoot,
    { childList: true, subtree: true },
    () => component.lists.length === URLs.length
  );
  ok(true, "There is a card for each site.");

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
  await BrowserTestUtils.waitForMutationCondition(
    component.shadowRoot,
    { childList: true, subtree: true },
    () => component.lists.length === dates.length
  );
  ok(true, "There is a card for each date.");
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
  const { component, contentWindow } = await showHistorySidebar();
  const { lists, cards } = component;
  await BrowserTestUtils.waitForMutationCondition(
    component.shadowRoot,
    { childList: true, subtree: true },
    () => !!lists.length
  );
  await BrowserTestUtils.waitForMutationCondition(
    lists[0].shadowRoot,
    { subtree: true, childList: true },
    () => lists[0].rowEls.length === URLs.length
  );
  ok(true, "History rows are shown.");
  const rows = lists[0].rowEls;

  cards[0].summaryEl.focus();

  info("Focus the next row.");
  let focused = BrowserTestUtils.waitForEvent(rows[0], "focus", contentWindow);
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, contentWindow);
  await focused;

  info("Focus the previous card.");
  focused = BrowserTestUtils.waitForEvent(
    cards[0].summaryEl,
    "focus",
    contentWindow
  );
  EventUtils.synthesizeKey("KEY_ArrowUp", {}, contentWindow);
  await focused;

  info("Focus the next row.");
  focused = BrowserTestUtils.waitForEvent(rows[0], "focus", contentWindow);
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, contentWindow);
  await focused;

  info("Focus the next row.");
  focused = BrowserTestUtils.waitForEvent(rows[1], "focus", contentWindow);
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, contentWindow);
  await focused;

  info("Focus the next row.");
  focused = BrowserTestUtils.waitForEvent(rows[2], "focus", contentWindow);
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, contentWindow);
  await focused;

  info("Focus the next row.");
  focused = BrowserTestUtils.waitForEvent(rows[3], "focus", contentWindow);
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, contentWindow);
  await focused;

  info("Focus the next card.");
  focused = BrowserTestUtils.waitForEvent(
    cards[1].summaryEl,
    "focus",
    contentWindow
  );
  EventUtils.synthesizeKey("KEY_ArrowDown", {}, contentWindow);
  await focused;

  info("Focus the previous row.");
  focused = BrowserTestUtils.waitForEvent(rows[3], "focus", contentWindow);
  EventUtils.synthesizeKey("KEY_ArrowUp", {}, contentWindow);
  await focused;

  info("Open the focused link.");
  await waitForPageLoadTask(
    () => EventUtils.synthesizeKey("KEY_Enter", {}, contentWindow),
    URLs[1]
  );
  win.SidebarController.hide();
});

add_task(async function test_history_hover_buttons() {
  const { component, contentWindow } = await showHistorySidebar();
  const { lists } = component;
  await BrowserTestUtils.waitForMutationCondition(
    component.shadowRoot,
    { childList: true, subtree: true },
    () => !!lists.length
  );
  await BrowserTestUtils.waitForMutationCondition(
    lists[0].shadowRoot,
    { subtree: true, childList: true },
    () => lists[0].rowEls.length === URLs.length
  );
  ok(true, "History rows are shown.");
  const rows = lists[0].rowEls;

  info("Open the first link.");
  await waitForPageLoadTask(
    () => EventUtils.synthesizeMouseAtCenter(rows[0].mainEl, {}, contentWindow),
    URLs[1]
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
  const { component } = await showHistorySidebar();
  const { lists } = component;
  await BrowserTestUtils.waitForMutationCondition(
    component.shadowRoot,
    { childList: true, subtree: true },
    () => !!lists.length
  );
  await BrowserTestUtils.waitForMutationCondition(
    lists[0].shadowRoot,
    { subtree: true, childList: true },
    () => !!lists[0].rowEls.length
  );
  ok(true, "History rows are shown.");
  const contextMenu = win.SidebarController.currentContextMenu;
  let rows = lists[0].rowEls;

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

  info("Open link in new tab.");
  const promiseTabOpen = BrowserTestUtils.waitForEvent(
    win.gBrowser.tabContainer,
    "TabOpen"
  );
  await openAndWaitForContextMenu(contextMenu, rows[0].mainEl, () =>
    contextMenu.activateItem(getItem("open-in-tab"))
  );
  await promiseTabOpen;
  await BrowserTestUtils.browserLoaded(
    win.gBrowser,
    false,
    rows[0].mainEl.href
  );
  is(win.gBrowser.currentURI.spec, rows[0].mainEl.href, "New tab opened");

  info("Clear all data from website");
  const promptPromise = PromptTestUtils.handleNextPrompt(
    win.SidebarController.browser,
    { modalType: Services.prompt.MODAL_TYPE_WINDOW, promptType: "confirmEx" },
    { buttonNumClick: 0 }
  );
  const promiseForgotten = PlacesTestUtils.waitForNotification("page-removed");
  await openAndWaitForContextMenu(contextMenu, rows[0].mainEl, () =>
    contextMenu.activateItem(getItem("forget-site"))
  );
  await promptPromise;
  await promiseForgotten;
  await TestUtils.waitForCondition(
    () => lists[0].rowEls.length === URLs.length - 3,
    "The forgotten entry should no longer be visible."
  );

  info("Open new container tab");
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(win.gBrowser, null);
  rows[0].mainEl.scrollIntoView();
  const eventDetails = { type: "contextmenu", button: 2 };
  info("Wait for context menu");
  let shown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(
    rows[0].mainEl,
    eventDetails,
    // eslint-disable-next-line mozilla/use-ownerGlobal
    rows[0].mainEl.ownerDocument.defaultView
  );
  await shown;
  let hidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  let containerContextMenu = win.document.getElementById(
    "sidebar-history-context-menu-container-tab"
  );
  let menuPopup = containerContextMenu.menupopup;
  info("Wait for container sub menu");
  let menuPopupPromise = BrowserTestUtils.waitForEvent(menuPopup, "popupshown");
  containerContextMenu.openMenu(true);
  await menuPopupPromise;
  info("Click first child to open a tab in a container");
  contextMenu.activateItem(menuPopup.childNodes[0]);
  await hidden;
  await promiseTabOpened;

  info("Add new bookmark");
  await withBookmarksDialog(
    false,
    async () => {
      // Open the context menu.
      await openAndWaitForContextMenu(contextMenu, rows[0].mainEl, () =>
        contextMenu.activateItem(getItem("bookmark-tab"))
      );
    },
    async dialogWin => {
      fillBookmarkTextField(
        "editBMPanel_locationField",
        rows[0].mainEl.href,
        dialogWin,
        false
      );
      EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
    }
  );
  await toggleSidebarPanel(win, "viewBookmarksSidebar");
  let tree =
    win.SidebarController.browser.contentDocument.getElementById(
      "bookmarks-view"
    );
  tree._view._nodeDetails.get("place:parent=toolbar_____*0*-1").containerOpen =
    true;
  let vals = [];
  tree._view._nodeDetails.values().forEach(val => vals.push(val.uri));
  ok(vals.includes(rows[0].mainEl.href), "Bookmark entry exists");
  await PlacesUtils.bookmarks.eraseEverything();

  // clean up extra tabs
  while (win.gBrowser.tabs.length > 1) {
    await BrowserTestUtils.removeTab(win.gBrowser.tabs.at(-1));
  }
  win.SidebarController.hide();
});

add_task(async function test_history_empty_state() {
  const { component } = await showHistorySidebar();
  info("Clear all history.");
  await PlacesUtils.history.clear();
  info("Waiting for history empty state to be present");
  await BrowserTestUtils.waitForMutationCondition(
    component.shadowRoot,
    { childList: true, subtree: true },
    () => !!component.emptyState
  );
  ok(
    BrowserTestUtils.isVisible(component.emptyState),
    "Empty state is displayed."
  );
  win.SidebarController.hide();
});
