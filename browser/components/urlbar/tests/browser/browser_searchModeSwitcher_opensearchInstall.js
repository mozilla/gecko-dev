/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ENGINE_TEST_URL =
  "http://mochi.test:8888/browser/browser/components/search/test/browser/opensearch.html";

let loadUri = async uri => {
  let loaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    uri
  );
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, uri);
  await loaded;
};

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });
});

add_task(async () => {
  await testInstallEngine(_popup => {
    EventUtils.synthesizeKey("KEY_Tab");
    EventUtils.synthesizeKey("KEY_Enter");
  });

  await testInstallEngine(popup => {
    popup.querySelector("toolbarbutton[label=engine1]").click();
  });
});

async function testInstallEngine(installFun) {
  info("Test installing opensearch engine");
  await loadUri(ENGINE_TEST_URL);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });

  let promiseEngineAdded = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ADDED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  let popup = await UrlbarTestUtils.openSearchModeSwitcher(window);
  await Promise.all([installFun(popup), promiseEngineAdded]);
  Assert.ok(true, "The engine was installed.");

  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "Foo",
    entry: "searchbutton",
  });

  await UrlbarTestUtils.exitSearchMode(window, {
    clickClose: true,
    waitForSearch: false,
  });

  await UrlbarTestUtils.promisePopupClose(window);

  let promiseEngineRemoved = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.REMOVED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  let settingsWritten = SearchTestUtils.promiseSearchNotification(
    "write-settings-to-disk-complete"
  );
  let engine = Services.search.getEngineByName("Foo");
  await Promise.all([
    Services.search.removeEngine(engine),
    promiseEngineRemoved,
    settingsWritten,
  ]);
}
