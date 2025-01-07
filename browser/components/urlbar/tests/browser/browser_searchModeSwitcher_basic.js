/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function setup() {
  requestLongerTimeout(5);
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });
});

add_task(async function open_settings() {
  let popup = UrlbarTestUtils.searchModeSwitcherPopup(window);
  let promiseMenuOpen = BrowserTestUtils.waitForEvent(popup, "popupshown");

  info("Open the urlbar and open the switcher via Enter key");
  await focusSwitcher();
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
  await focusSwitcher(newWin);
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
    entry: "searchbutton",
    source: 3,
  });

  info("Press the close button and escape search mode");
  window.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.assertSearchMode(window, null);
});

add_task(async function privileged_chicklet() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "about:config"
  );

  Assert.ok(
    BrowserTestUtils.isVisible(
      tab.ownerGlobal.document.querySelector("#identity-box")
    ),
    "Chicklet is visible on privileged pages."
  );

  BrowserTestUtils.removeTab(tab);
});

function updateEngine(fun) {
  let updated = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  fun();
  return updated;
}

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
    entry: "searchbutton",
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

async function setDefaultEngine(name) {
  let engine = (await Services.search.getEngines()).find(e => e.name == name);
  Assert.ok(engine);
  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
}

add_task(async function test_search_icon_change() {
  await SpecialPowers.pushPrefEnv({
    set: [["keyword.enabled", false]],
  });

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  const searchGlassIconUrl = UrlbarUtils.ICON.SEARCH_GLASS;

  Assert.equal(
    getSeachModeSwitcherIcon(newWin),
    searchGlassIconUrl,
    "The search mode switcher should have the search glass icon url since \
     we are not in search mode."
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

  Assert.equal(
    getSeachModeSwitcherIcon(newWin),
    bingSearchEngineIconUrl,
    "The search mode switcher should have the bing icon url since we are in \
     search mode"
  );
  await UrlbarTestUtils.assertSearchMode(newWin, {
    engineName: "Bing",
    entry: "searchbutton",
    source: 3,
  });

  info("Press the close button and exit search mode");
  newWin.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.assertSearchMode(newWin, null);

  let searchModeSwitcherIconUrl = await BrowserTestUtils.waitForCondition(
    () => getSeachModeSwitcherIcon(newWin),
    "Waiting for the search mode switcher icon to update after exiting search mode."
  );

  Assert.equal(
    searchModeSwitcherIconUrl,
    searchGlassIconUrl,
    "The search mode switcher should have the search glass icon url since \
     keyword.enabled is false"
  );

  await BrowserTestUtils.closeWindow(newWin);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_suggestions_after_no_search_mode() {
  info("Add a search engine as default");
  let defaultEngine = await SearchTestUtils.installSearchExtension(
    {
      name: "default-engine",
      search_url: "https://www.example.com/",
      favicon_url: "https://www.example.com/favicon.ico",
    },
    {
      setAsDefault: true,
      skipUnload: true,
    }
  );

  info("Add one more search engine to check the result");
  let anotherEngine = await SearchTestUtils.installSearchExtension(
    {
      name: "another-engine",
      search_url: "https://example.com/",
      favicon_url: "https://example.com/favicon.ico",
    },
    { skipUnload: true }
  );

  info("Open urlbar with a query");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  Assert.equal(
    (await UrlbarTestUtils.getDetailsOfResultAt(window, 0)).result.payload
      .engine,
    "default-engine",
    "Suggest to search from the default engine"
  );

  info("Open search mode swither");
  let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);

  info("Press on the another-engine menu button");
  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(window);
  popup.querySelector("toolbarbutton[label=another-engine]").click();
  await popupHidden;
  Assert.equal(
    (await UrlbarTestUtils.getDetailsOfResultAt(window, 0)).result.payload
      .engine,
    "another-engine",
    "Suggest to search from the another engine"
  );

  info("Press the close button and escape search mode");
  window.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.assertSearchMode(window, null);
  Assert.equal(
    (await UrlbarTestUtils.getDetailsOfResultAt(window, 0)).result.payload
      .engine,
    "default-engine",
    "Suggest to search from the default engine again"
  );

  await defaultEngine.unload();
  await anotherEngine.unload();
});

add_task(async function open_engine_page_directly() {
  let searchExtension = await SearchTestUtils.installSearchExtension(
    {
      name: "MozSearch",
      search_url: "https://example.com/",
      favicon_url: "https://example.com/favicon.ico",
    },
    { setAsDefault: true, skipUnload: true }
  );

  const TEST_DATA = [
    {
      action: "click",
      input: "",
      expected: "https://example.com/",
    },
    {
      action: "click",
      input: "a b c",
      expected: "https://example.com/?q=a+b+c",
    },
    {
      action: "key",
      input: "",
      expected: "https://example.com/",
    },
    {
      action: "key",
      input: "a b c",
      expected: "https://example.com/?q=a+b+c",
    },
  ];

  for (let { action, input, expected } of TEST_DATA) {
    info(`Test for ${JSON.stringify({ action, input, expected })}`);

    info("Open a window");
    let newWin = await BrowserTestUtils.openNewBrowserWindow();

    info(`Open the result popup with [${input}]`);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: newWin,
      value: input,
    });

    info("Open the mode switcher");
    let popup = await UrlbarTestUtils.openSearchModeSwitcher(newWin);

    info(`Do action of [${action}] on MozSearch menuitem`);
    let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(newWin);
    let pageLoaded = BrowserTestUtils.browserLoaded(
      newWin.gBrowser.selectedBrowser,
      false,
      expected
    );

    if (action == "click") {
      EventUtils.synthesizeMouseAtCenter(
        popup.querySelector("toolbarbutton[label=MozSearch]"),
        {
          shiftKey: true,
        },
        newWin
      );
    } else {
      popup.querySelector("toolbarbutton[label=MozSearch]").focus();
      EventUtils.synthesizeKey("KEY_Enter", { shiftKey: true }, newWin);
    }

    await popupHidden;
    await pageLoaded;
    Assert.ok(true, "The popup was hidden and expected page was loaded");

    await UrlbarTestUtils.assertSearchMode(newWin, null);

    // Cleanup.
    await PlacesUtils.history.clear();
    await BrowserTestUtils.closeWindow(newWin);
  }
  await searchExtension.unload();
});

add_task(async function test_enter_searchmode_by_key_if_single_result() {
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "https://example.com/",
    title: "BOOKMARK",
  });

  const TEST_DATA = [
    {
      key: "KEY_Enter",
      expectedEntry: "keywordoffer",
    },
    {
      key: "KEY_Tab",
      expectedEntry: "keywordoffer",
    },
    {
      key: "VK_RIGHT",
      expectedEntry: "typed",
    },
    {
      key: "VK_DOWN",
      expectedEntry: "keywordoffer",
    },
  ];
  for (let { key, expectedEntry } of TEST_DATA) {
    info(`Test for entering search mode by ${key}`);

    info("Open urlbar with a query that shows bookmarks");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "@book",
    });

    // Sanity check.
    const autofill = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.equal(autofill.result.providerName, "RestrictKeywordsAutofill");
    Assert.equal(autofill.result.payload.autofillKeyword, "@bookmarks");

    info("Choose the search mode suggestion");
    EventUtils.synthesizeKey(key, {});
    await UrlbarTestUtils.promiseSearchComplete(window);
    await UrlbarTestUtils.assertSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      entry: expectedEntry,
      restrictType: "keyword",
    });

    info("Check the suggestions");
    Assert.equal(UrlbarTestUtils.getResultCount(window), 1);
    const bookmark = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.equal(bookmark.result.source, UrlbarUtils.RESULT_SOURCE.BOOKMARKS);
    Assert.equal(bookmark.result.type, UrlbarUtils.RESULT_TYPE.URL);
    Assert.equal(bookmark.result.payload.url, "https://example.com/");
    Assert.equal(bookmark.result.payload.title, "BOOKMARK");

    info("Choose any search engine from the switcher");
    let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);
    let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(window);
    popup.querySelector("toolbarbutton[label=Bing]").click();
    await popupHidden;
    Assert.equal(gURLBar.value, "", "The value of urlbar should be empty");

    // Clean up.
    window.document.querySelector("#searchmode-switcher-close").click();
    await UrlbarTestUtils.assertSearchMode(window, null);
  }

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(
  async function test_enter_searchmode_as_preview_by_key_if_multiple_results() {
    await PlacesTestUtils.addBookmarkWithDetails({
      uri: "https://example.com/",
      title: "BOOKMARK",
    });

    for (let key of ["KEY_Tab", "VK_DOWN"]) {
      info(`Test for entering search mode by ${key}`);

      info("Open urlbar with a query that shows bookmarks");
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "@",
      });

      info("Choose the bookmark search mode");
      let resultCount = UrlbarTestUtils.getResultCount(window);
      for (let i = 0; i < resultCount; i++) {
        EventUtils.synthesizeKey(key, {});

        let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
        if (
          result.providerName == "RestrictKeywords" &&
          result.payload.keyword == "*"
        ) {
          await UrlbarTestUtils.assertSearchMode(window, {
            source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
            entry: "keywordoffer",
            restrictType: "keyword",
            isPreview: true,
          });
          break;
        }
      }

      // Clean up.
      window.document.querySelector("#searchmode-switcher-close").click();
      await UrlbarTestUtils.assertSearchMode(window, null);
    }

    await PlacesUtils.bookmarks.eraseEverything();
  }
);

add_task(async function test_open_state() {
  let popup = UrlbarTestUtils.searchModeSwitcherPopup(window);
  let switcher = document.getElementById("urlbar-searchmode-switcher");

  for (let target of [
    "urlbar-searchmode-switcher",
    "searchmode-switcher-icon",
    "searchmode-switcher-dropmarker",
  ]) {
    info(`Open search mode switcher popup by clicking on [${target}]`);
    let popupOpen = BrowserTestUtils.waitForEvent(popup, "popupshown");
    let button = document.getElementById(target);
    button.click();
    await popupOpen;
    Assert.equal(
      switcher.getAttribute("open"),
      "true",
      "The 'open' attribute should be true"
    );

    info("Close the popup");
    popup.hidePopup();
    await TestUtils.waitForCondition(() => {
      return !switcher.hasAttribute("open");
    });
    Assert.ok(true, "The 'open' attribute should not be set");
  }
});

add_task(async function nimbusScotchBonnetEnableOverride() {
  info("Setup initial local pref");
  let defaultBranch = Services.prefs.getDefaultBranch("browser.urlbar.");
  let initialValue = defaultBranch.getBoolPref("scotchBonnet.enableOverride");
  defaultBranch.setBoolPref("scotchBonnet.enableOverride", false);
  UrlbarPrefs.clear("scotchBonnet.enableOverride");

  await TestUtils.waitForCondition(() => {
    return BrowserTestUtils.isHidden(
      gURLBar.querySelector("#urlbar-searchmode-switcher")
    );
  });
  Assert.ok(true, "Search mode switcher should be hidden");

  info("Setup Numbus value");
  const cleanUpNimbusEnable = await UrlbarTestUtils.initNimbusFeature(
    { scotchBonnetEnableOverride: true },
    "search"
  );
  await TestUtils.waitForCondition(() => {
    return BrowserTestUtils.isVisible(
      gURLBar.querySelector("#urlbar-searchmode-switcher")
    );
  });
  Assert.ok(true, "Search mode switcher should be visible");

  await cleanUpNimbusEnable();
  defaultBranch.setBoolPref("scotchBonnet.enableOverride", initialValue);
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });
});

add_task(async function nimbusLogEnabled() {
  info("Setup initial local pref");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.log", false]],
  });
  await TestUtils.waitForCondition(() => {
    return !Services.prefs.getBoolPref("browser.search.log");
  });

  info("Setup Numbus value");
  const cleanUpNimbusEnable = await UrlbarTestUtils.initNimbusFeature(
    { logEnabled: true },
    "search"
  );
  await TestUtils.waitForCondition(() => {
    return Services.prefs.getBoolPref("browser.search.log");
  });
  Assert.ok(true, "browser.search.log is changed properly");

  await cleanUpNimbusEnable();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_button_stuck() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let popup = win.document.getElementById("searchmode-switcher-popup");
  let button = win.document.getElementById("urlbar-searchmode-switcher");

  info("Show the SearchModeSwitcher");
  let promiseMenuOpen = BrowserTestUtils.waitForEvent(popup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(button, {}, win);
  await promiseMenuOpen;

  info("Hide the SearchModeSwitcher");
  let promiseMenuClosed = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  EventUtils.synthesizeMouseAtCenter(button, {}, win);
  await promiseMenuClosed;
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_readonly() {
  let popupOpened = BrowserTestUtils.waitForNewWindow({ url: "about:blank" });
  BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html,<html><script>popup=open('about:blank','','width=300,height=200')</script>"
  );
  let win = await popupOpened;

  Assert.ok(win.gURLBar, "location bar exists in the popup");
  Assert.ok(win.gURLBar.readOnly, "location bar is read-only in the popup");

  Assert.equal(
    BrowserTestUtils.isVisible(
      win.gURLBar.querySelector("#urlbar-searchmode-switcher")
    ),
    false,
    "Unified Search Button should not be visible in readonly windows"
  );

  let closedPopupPromise = BrowserTestUtils.windowClosed(win);
  win.close();
  await closedPopupPromise;
  gBrowser.removeCurrentTab();
});

add_task(async function test_search_service_fail() {
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  const stub = sinon
    .stub(UrlbarSearchUtils, "init")
    .rejects(new Error("Initialization failed"));

  Services.search.wrappedJSObject.forceInitializationStatusForTests(
    "not initialized"
  );

  // Force updateSearchIcon to be triggered
  await SpecialPowers.pushPrefEnv({
    set: [["keyword.enabled", false]],
  });

  let searchModeSwitcherIconUrl = await BrowserTestUtils.waitForCondition(
    () => getSeachModeSwitcherIcon(newWin),
    "Waiting for the search mode switcher icon to update after exiting search mode."
  );

  Assert.equal(
    searchModeSwitcherIconUrl,
    UrlbarUtils.ICON.SEARCH_GLASS,
    "The search mode switcher should have the search glass icon url since the search service init failed."
  );

  info("Open search mode switcher");
  let popup = await UrlbarTestUtils.openSearchModeSwitcher(newWin);

  info("Ensure local search modes are present in popup");
  let localSearchModes = ["bookmarks", "history", "tabs"];
  for (let searchMode of localSearchModes) {
    popup.querySelector(`#search-button-${searchMode}`);
    Assert.ok("Local search modes should be present");
  }

  let localSearchButton = popup.querySelector(
    `#search-button-${localSearchModes[0]}`
  );

  let popupHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  localSearchButton.click();
  await popupHidden;

  stub.restore();

  Services.search.wrappedJSObject.forceInitializationStatusForTests("success");

  await BrowserTestUtils.closeWindow(newWin);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_search_mode_switcher_engine_no_icon() {
  const testEngineName = "TestEngineNoIcon";
  let searchExtension = await SearchTestUtils.installSearchExtension(
    {
      name: testEngineName,
      search_url: "https://www.example.com/search?q=",
      favicon_url: "",
    },
    { skipUnload: true }
  );

  let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);

  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(window);
  popup.querySelector(`toolbarbutton[label=${testEngineName}]`).click();
  await popupHidden;

  Assert.equal(
    getSeachModeSwitcherIcon(window),
    UrlbarUtils.ICON.SEARCH_GLASS,
    "The search mode switcher should display the default search glass icon when the engine has no icon."
  );

  info("Press the close button and escape search mode");
  window.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.assertSearchMode(window, null);

  await searchExtension.unload();
});

add_task(async function test_search_mode_switcher_private_engine_icon() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.separatePrivateDefault.ui.enabled", true]],
  });

  const testEngineName = "DefaultPrivateEngine";
  let searchExtension = await SearchTestUtils.installSearchExtension(
    {
      name: testEngineName,
      search_url: "https://www.example.com/search?q=",
      icons: {
        16: "private.png",
      },
    },
    { skipUnload: true }
  );

  const defaultPrivateEngine = Services.search.getEngineByName(testEngineName);
  const defaultPrivateEngineIcon = `moz-extension://${searchExtension.uuid}/private.png`;
  const defaultEngine = await Services.search.getDefault();
  const defaultEngineIcon = await defaultEngine.getIconURL();

  Services.search.setDefaultPrivate(
    defaultPrivateEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  Assert.notEqual(
    defaultEngine.id,
    defaultPrivateEngine.id,
    "Default engine is not private engine."
  );
  Assert.equal(
    (await Services.search.getDefault()).id,
    defaultEngine.id,
    "Default engine is still correct."
  );
  Assert.equal(
    (await Services.search.getDefaultPrivate()).id,
    defaultPrivateEngine.id,
    "Default private engine is correct."
  );

  Assert.equal(
    getSeachModeSwitcherIcon(window),
    defaultEngineIcon,
    "Is the icon of the default engine."
  );

  info("Open a private window");
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  Assert.equal(
    getSeachModeSwitcherIcon(privateWin),
    defaultPrivateEngineIcon,
    "Is the icon of the default private engine."
  );

  info("Changing the default private engine.");
  Services.search.setDefaultPrivate(
    defaultEngine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  info("Waiting for the icon to be updated.");
  await TestUtils.waitForCondition(
    () => getSeachModeSwitcherIcon(privateWin) == defaultEngineIcon
  );
  Assert.ok(true, "The icon was updated.");

  await BrowserTestUtils.closeWindow(privateWin);
  await searchExtension.unload();
  await SpecialPowers.popPrefEnv();
});

function getSeachModeSwitcherIcon(window) {
  let searchModeSwitcherButton = window.document.getElementById(
    "searchmode-switcher-icon"
  );

  // match and capture the URL inside `url("...")`
  let re = /url\("([^"]+)"\)/;
  return searchModeSwitcherButton.style.listStyleImage.match(re)?.[1] ?? null;
}
