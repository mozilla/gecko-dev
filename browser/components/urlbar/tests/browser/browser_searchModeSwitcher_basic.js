/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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

async function setDefaultEngine(name) {
  let engine = (await Services.search.getEngines()).find(e => e.name == name);
  Assert.ok(engine);
  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
}

add_task(async function test_search_icon_change() {
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
  EventUtils.synthesizeKey("KEY_Escape");
  EventUtils.synthesizeKey("KEY_Escape");
  await UrlbarTestUtils.assertSearchMode(window, null);

  await BrowserTestUtils.waitForCondition(() => {
    return (
      window.document.querySelector("#searchmode-switcher-title").textContent ==
      ""
    );
  }, "The searchMode name has been removed when we exit search mode");
});
