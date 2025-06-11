/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });

  let oldDefaultEngine = await Services.search.getDefault();

  await SearchTestUtils.installSearchExtension(
    {
      name: "Example",
      search_url: "https://example.com/search",
      search_url_get_params: "q={searchTerms}",
    },
    { setAsDefault: true }
  );

  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits({
    uri: "https://example.com/search?q=testing",
    title: "testing",
  });

  registerCleanupFunction(async function () {
    await Services.search.setDefault(
      oldDefaultEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_engine_searchmode_after_result_nav() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  info("Open the urlbar and searchmode switcher popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  EventUtils.synthesizeKey("KEY_ArrowDown");

  let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);
  Assert.ok(
    !BrowserTestUtils.isVisible(gURLBar.view.panel),
    "The UrlbarView is not visible"
  );

  info("Press on the bing menu button and enter search mode");
  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(window);
  popup.querySelector("menuitem[label=Example]").click();
  await popupHidden;

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "Example",
    entry: "searchbutton",
  });

  Assert.equal(gURLBar.value, "https://example.com/search?q=testing");

  info("Press the close button and escape search mode");
  window.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.assertSearchMode(window, null);
  gBrowser.removeTab(tab);
});

add_task(async function test_local_searchmode_after_result_nav() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  info("Open the urlbar and searchmode switcher popup");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  EventUtils.synthesizeKey("KEY_ArrowDown");

  let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);
  Assert.ok(
    !BrowserTestUtils.isVisible(gURLBar.view.panel),
    "The UrlbarView is not visible"
  );

  info("Press on the bing menu button and enter search mode");
  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(window);
  popup.querySelector("menuitem[label=Bookmarks]").click();
  await popupHidden;

  await UrlbarTestUtils.assertSearchMode(window, {
    source: 1,
    entry: "searchbutton",
  });

  Assert.equal(gURLBar.value, "https://example.com/search?q=testing");

  info("Press the close button and escape search mode");
  window.document.querySelector("#searchmode-switcher-close").click();
  await UrlbarTestUtils.assertSearchMode(window, null);

  gBrowser.removeTab(tab);
});
