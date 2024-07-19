/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });
});

add_task(async function basic() {
  let popup = UrlbarTestUtils.searchModeSwitcherPopup(window);

  info("Open the urlbar and searchmode switcher popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  await UrlbarTestUtils.openSearchModeSwitcher(window);
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

  info("Press the close button and exit search mode");
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
  let popup = UrlbarTestUtils.searchModeSwitcherPopup(newWin);

  info("Open the urlbar and searchmode switcher popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: newWin,
    value: "",
  });
  await UrlbarTestUtils.openSearchModeSwitcher(newWin);

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
