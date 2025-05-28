/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry for the heuristicResultMissing Glean metric.
 *
 * This metric is recorded when handleNavigation is called.
 * This occurs in the following cases:
 * - When the user copies text into the clipboard bar and selects "Paste and Go" in the URL bar.
 * - When the user drags text into the URL bar.
 * - When the user presses enter in the URL bar.
 * - When the user selects a one-off search engine.
 */

"use strict";

add_setup(async function () {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  // Install a custom search engine and set it as the default.
  await SearchTestUtils.installSearchExtension(
    {
      search_url: "https://example.com/search",
      search_url_get_params: "q={searchTerms}",
      suggest_url:
        "https://example.com/browser/browser/components/search/test/browser/searchSuggestionEngine.sjs",
      suggest_url_get_params: "query={searchTerms}",
    },
    { setAsDefault: true }
  );
});

add_task(async function test_pasteAndGo_heuristicResultMissingNoStub() {
  let searchString = "test";

  gURLBar.focus();

  await SimpleTest.promiseClipboardChange(searchString, () => {
    clipboardHelper.copyString(searchString);
  });

  let menuitem = await promiseContextualMenuitem("paste-and-go");
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    window.gBrowser,
    false
  );
  menuitem.closest("menupopup").activateItem(menuitem);

  await browserLoadedPromise;

  let value = Glean.urlbar.heuristicResultMissing.testGetValue();

  Assert.deepEqual(
    value,
    { numerator: 0, denominator: 1 },
    "Should have recorded a denominator hit for pasteAndGo"
  );

  Services.fog.testResetFOG();
});

add_task(async function test_pasteAndGo_heuristicResultMissing() {
  const stub = sinon
    .stub(UrlbarUtils, "getHeuristicResultFor")
    .rejects(new Error("Initialization failed"));

  let searchString = "test";

  gURLBar.focus();

  await SimpleTest.promiseClipboardChange(searchString, () => {
    clipboardHelper.copyString(searchString);
  });

  let menuitem = await promiseContextualMenuitem("paste-and-go");
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    window.gBrowser,
    false
  );
  menuitem.closest("menupopup").activateItem(menuitem);

  await browserLoadedPromise;

  stub.restore();

  let value = Glean.urlbar.heuristicResultMissing.testGetValue();

  Assert.deepEqual(
    value,
    { numerator: 1, denominator: 1 },
    "Should have recorded a numerator and denominator hit for pasteAndGo"
  );

  Services.fog.testResetFOG();
});

add_task(async function test_dragText_heuristicResultMissing() {
  const stub = sinon
    .stub(UrlbarUtils, "getHeuristicResultFor")
    .rejects(new Error("Initialization failed"));

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html,<p id='text'>Drag this text</p>"
  );

  let draggedText = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => content.document.getElementById("text").textContent
  );

  let dataTransfer = new DataTransfer();
  dataTransfer.setData("text/plain", draggedText);

  let input = gURLBar.inputField;

  let dragEnterEvent = new DragEvent("dragenter", {
    bubbles: true,
    cancelable: true,
    dataTransfer,
  });
  input.dispatchEvent(dragEnterEvent);

  let dropEvent = new DragEvent("drop", {
    bubbles: true,
    cancelable: true,
    dataTransfer,
  });
  input.dispatchEvent(dropEvent);

  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  stub.restore();

  let value = Glean.urlbar.heuristicResultMissing.testGetValue();

  Assert.deepEqual(
    value,
    { numerator: 1, denominator: 1 },
    "Should have recorded a numerator and denominator hit for dragText"
  );

  BrowserTestUtils.removeTab(tab);

  Services.fog.testResetFOG();
});

add_task(async function test_enterKey_heuristicResultMissing() {
  const stub = sinon
    .stub(UrlbarUtils, "getHeuristicResultFor")
    .rejects(new Error("Initialization failed"));

  gURLBar.focus();
  gURLBar.value = "test";
  EventUtils.synthesizeKey("KEY_Enter");

  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  stub.restore();

  let value = Glean.urlbar.heuristicResultMissing.testGetValue();

  Assert.deepEqual(
    value,
    { numerator: 1, denominator: 1 },
    "Should have recorded a numerator and denominator hit for enterKey"
  );

  Services.fog.testResetFOG();
});

add_task(async function test_oneOffSearch_heuristicResultMissing() {
  const stub = sinon
    .stub(UrlbarUtils, "getHeuristicResultFor")
    .rejects(new Error("Initialization failed"));

  gURLBar.focus();
  gURLBar.value = "@test";

  EventUtils.synthesizeKey("KEY_Enter");

  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  stub.restore();

  let value = Glean.urlbar.heuristicResultMissing.testGetValue();

  Assert.deepEqual(
    value,
    { numerator: 1, denominator: 1 },
    "Should have recorded a numerator and denominator hit for oneOffSearch"
  );

  Services.fog.testResetFOG();
});
