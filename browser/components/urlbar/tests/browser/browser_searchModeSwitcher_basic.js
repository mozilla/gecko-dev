/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });
});

add_task(async function basic() {
  info("Open the urlbar and searchmode switcher popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);
  Assert.ok(
    !BrowserTestUtils.isVisible(gURLBar.view.panel),
    "The UrlbarView is not visible"
  );

  info("Press on the bing menu button and enter search mode");
  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(window);
  popup.querySelector("toolbarbutton[label=Bing]").click();
  await popupHidden;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "Bing",
    entry: "other",
    source: 3,
  });

  info("Press the close button and escape search mode");
  window.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.assertSearchMode(window, null);
});

function updateEngine(fun) {
  let updated = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  fun();
  return updated;
}

add_task(async function disabled_unified_button() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", false]],
  });

  await TestUtils.waitForCondition(() => {
    return !BrowserTestUtils.isVisible(
      gURLBar.querySelector("#urlbar-searchmode-switcher")
    );
  });

  Assert.equal(
    BrowserTestUtils.isVisible(
      gURLBar.querySelector("#urlbar-searchmode-switcher")
    ),
    false,
    "Unified Search Button should not be visible."
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  Assert.equal(
    BrowserTestUtils.isVisible(
      gURLBar.querySelector("#urlbar-searchmode-switcher")
    ),
    false,
    "Unified Search Button should not be visible."
  );

  await UrlbarTestUtils.enterSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  });

  Assert.equal(
    BrowserTestUtils.isVisible(
      gURLBar.querySelector("#searchmode-switcher-chicklet")
    ),
    false,
    "Chicklet associated with Unified Search Button should not be visible."
  );

  await UrlbarTestUtils.exitSearchMode(window);
  await SpecialPowers.popPrefEnv();
});

add_task(async function new_window() {
  let oldEngine = Services.search.getEngineByName("Bing");
  await updateEngine(() => {
    oldEngine.hidden = true;
  });

  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  info("Open the urlbar and searchmode switcher popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: newWin,
    value: "",
  });
  let popup = await UrlbarTestUtils.openSearchModeSwitcher(newWin);

  info("Open popup and check list of engines is redrawn");
  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(newWin);
  Assert.ok(
    !popup.querySelector(`toolbarbutton[label=${oldEngine.name}]`),
    "List has been redrawn"
  );
  popup.querySelector("toolbarbutton[label=Google]").click();
  await popupHidden;
  newWin.document.querySelector("#searchmode-switcher-close").click();

  await Services.search.restoreDefaultEngines();
  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function detect_searchmode_changes() {
  info("Open the urlbar and searchmode switcher popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);

  info("Press on the bing menu button and enter search mode");
  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(window);
  popup.querySelector("toolbarbutton[label=Bing]").click();
  await popupHidden;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "Bing",
    entry: "other",
    source: 3,
  });

  info("Press the close button and escape search mode");
  window.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.assertSearchMode(window, null);

  await BrowserTestUtils.waitForCondition(() => {
    return (
      window.document.querySelector("#searchmode-switcher-title").textContent ==
      ""
    );
  }, "The searchMode name has been removed when we exit search mode");
});

function focusSwitcher(win = window) {
  EventUtils.synthesizeKey("l", { accelKey: true }, win);
  EventUtils.synthesizeKey("KEY_Escape", {}, win);
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true }, win);
}

/**
 * Test we can open the SearchModeSwitcher with various keys
 *
 * @param {string} openKey - The keyboard character used to open the popup.
 */
async function test_open_switcher(openKey) {
  let popup = UrlbarTestUtils.searchModeSwitcherPopup(window);
  let promiseMenuOpen = BrowserTestUtils.waitForEvent(popup, "popupshown");

  info("Open the urlbar and open the switcher via keyboard");
  focusSwitcher();
  EventUtils.synthesizeKey(openKey);
  await promiseMenuOpen;

  EventUtils.synthesizeKey("KEY_Escape");
}

/**
 * Test that not all characters will open the SearchModeSwitcher
 *
 * @param {string} dontOpenKey - The keyboard character we will ignore.
 */
async function test_dont_open_switcher(dontOpenKey) {
  let popup = UrlbarTestUtils.searchModeSwitcherPopup(window);

  let popupOpened = false;
  let opened = () => {
    popupOpened = true;
  };
  info("Open the urlbar and open the switcher via keyboard");
  popup.addEventListener("popupshown", opened);
  focusSwitcher();
  EventUtils.synthesizeKey(dontOpenKey);

  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(r => setTimeout(r, 50));
  Assert.ok(!popupOpened, "The popup was not opened");
  popup.removeEventListener("popupshown", opened);
}

/**
 * Test we can navigate the SearchModeSwitcher with various keys
 *
 * @param {string} navKey - The keyboard character used to navigate.
 * @param {Int} navTimes - The number of times we press that key.
 * @param {object} searchMode - The searchMode that we expect to select.
 */
async function test_navigate_switcher(navKey, navTimes, searchMode) {
  let popup = UrlbarTestUtils.searchModeSwitcherPopup(window);
  let promiseMenuOpen = BrowserTestUtils.waitForEvent(popup, "popupshown");

  info("Open the urlbar and open the switcher via keyboard");
  focusSwitcher();
  EventUtils.synthesizeKey("KEY_Enter");
  await promiseMenuOpen;

  info("Select first result and enter search mode");
  for (let i = 0; i < navTimes; i++) {
    EventUtils.synthesizeKey(navKey);
  }
  EventUtils.synthesizeKey("KEY_Enter");

  await UrlbarTestUtils.assertSearchMode(window, searchMode);

  info("Press the close button and escape search mode");
  window.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.assertSearchMode(window, null);
}

// TODO: Don't let tests depend on the actual search config.
let amazonSearchMode = {
  engineName: "Amazon.com",
  entry: "other",
  isPreview: false,
  isGeneralPurposeEngine: true,
};
let bingSearchMode = {
  engineName: "Bing",
  isGeneralPurposeEngine: true,
  source: 3,
  isPreview: false,
  entry: "other",
};

add_task(async function test_keyboard_nav() {
  await test_open_switcher("KEY_Enter");
  await test_open_switcher("KEY_ArrowDown");
  await test_open_switcher(" ");

  await test_dont_open_switcher("a");
  await test_dont_open_switcher("KEY_ArrowUp");
  await test_dont_open_switcher("x");

  await test_navigate_switcher("KEY_Tab", 1, amazonSearchMode);
  await test_navigate_switcher("KEY_ArrowDown", 1, amazonSearchMode);
  await test_navigate_switcher("KEY_Tab", 2, bingSearchMode);
  await test_navigate_switcher("KEY_ArrowDown", 2, bingSearchMode);
});

add_task(async function open_settings() {
  let popup = UrlbarTestUtils.searchModeSwitcherPopup(window);
  let promiseMenuOpen = BrowserTestUtils.waitForEvent(popup, "popupshown");

  info("Open the urlbar and open the switcher via keyboard");
  focusSwitcher();
  EventUtils.synthesizeKey("KEY_Enter");
  await promiseMenuOpen;

  let pageLoaded = BrowserTestUtils.browserLoaded(window);
  EventUtils.synthesizeKey("KEY_ArrowUp");
  EventUtils.synthesizeKey("KEY_Enter");
  await pageLoaded;

  Assert.equal(
    window.gBrowser.selectedBrowser.currentURI.spec,
    "about:preferences#search",
    "Opened settings page"
  );

  // Clean up.
  let onLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.selectedBrowser.loadURI(Services.io.newURI("about:newtab"), {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  await onLoaded;
});

add_task(async function open_settings_with_there_is_already_opened_settings() {
  info("Open settings page in a tab");
  let startTab = gBrowser.selectedTab;
  let preferencesTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences#search"
  );
  gBrowser.selectedTab = startTab;

  info("Open new window");
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let popup = UrlbarTestUtils.searchModeSwitcherPopup(newWin);
  let promiseMenuOpen = BrowserTestUtils.waitForEvent(popup, "popupshown");

  info("Open the urlbar and open the switcher via keyboard in the new window");
  focusSwitcher(newWin);
  EventUtils.synthesizeKey("KEY_Enter", {}, newWin);
  await promiseMenuOpen;

  info(
    "Choose open settings item and wait until the window having perference page will get focus"
  );
  let onFocus = BrowserTestUtils.waitForEvent(window, "focus", true);
  EventUtils.synthesizeKey("KEY_ArrowUp", {}, newWin);
  EventUtils.synthesizeKey("KEY_Enter", {}, newWin);
  await onFocus;
  Assert.ok(true, "The window that has perference page got focus");

  await BrowserTestUtils.waitForCondition(
    () => window.gBrowser.selectedTab == preferencesTab
  );
  Assert.ok(true, "Focus opened settings page");

  BrowserTestUtils.removeTab(preferencesTab);
  await BrowserTestUtils.closeWindow(newWin);
});

async function setDefaultEngine(name) {
  let engine = (await Services.search.getEngines()).find(e => e.name == name);
  Assert.ok(engine);
  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
}

add_task(async function test_search_icon_change() {
  const defaultEngine = await Services.search.getDefault();
  const engineName = "DuckDuckGo";
  await setDefaultEngine(engineName);
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  let searchModeSwitcherButton = window.document.getElementById(
    "searchmode-switcher-icon"
  );

  // match and capture the URL inside `url("...")`
  let regex = /url\("([^"]+)"\)/;
  let searchModeSwitcherIconUrl =
    searchModeSwitcherButton.style.listStyleImage.match(regex);

  const defaultSearchEngineIconUrl = await Services.search
    .getEngineByName(engineName)
    .getIconURL();

  Assert.equal(
    searchModeSwitcherIconUrl[1],
    defaultSearchEngineIconUrl,
    "The search mode switcher should have the same icon as the default search engine"
  );

  await Services.search.setDefault(
    defaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function test_search_icon_change_without_keyword_enabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["keyword.enabled", false]],
  });

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let searchModeSwitcherButton = newWin.document.getElementById(
    "searchmode-switcher-icon"
  );

  let regex = /url\("([^"]+)"\)/;
  let searchModeSwitcherIconUrl =
    searchModeSwitcherButton.style.listStyleImage.match(regex);

  const searchGlassIconUrl = UrlbarUtils.ICON.SEARCH_GLASS;

  Assert.equal(
    searchModeSwitcherIconUrl[1],
    searchGlassIconUrl,
    "The search mode switcher should have the search glass icon url since \
     keyword.enabled is false and we are not in search mode."
  );

  let popup = UrlbarTestUtils.searchModeSwitcherPopup(newWin);
  let engineName = "Bing";
  info("Open the urlbar and searchmode switcher popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: newWin,
    value: "",
  });
  await UrlbarTestUtils.openSearchModeSwitcher(newWin);
  info("Press on the bing menu button and enter search mode");
  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(newWin);
  popup.querySelector(`toolbarbutton[label=${engineName}]`).click();
  await popupHidden;

  const bingSearchEngineIconUrl = await Services.search
    .getEngineByName(engineName)
    .getIconURL();

  searchModeSwitcherIconUrl =
    searchModeSwitcherButton.style.listStyleImage.match(regex);

  Assert.equal(
    searchModeSwitcherIconUrl[1],
    bingSearchEngineIconUrl,
    "The search mode switcher should have the bing icon url since we are in \
     search mode"
  );
  await UrlbarTestUtils.assertSearchMode(newWin, {
    engineName: "Bing",
    entry: "other",
    source: 3,
  });

  info("Press the close button and exit search mode");
  newWin.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.assertSearchMode(newWin, null);

  searchModeSwitcherIconUrl = await BrowserTestUtils.waitForCondition(
    () => searchModeSwitcherButton.style.listStyleImage.match(regex),
    "Waiting for the search mode switcher icon to update after exiting search mode."
  );

  Assert.equal(
    searchModeSwitcherIconUrl[1],
    searchGlassIconUrl,
    "The search mode switcher should have the search glass icon url since \
     keyword.enabled is false"
  );

  await BrowserTestUtils.closeWindow(newWin);
});
