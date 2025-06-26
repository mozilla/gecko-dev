/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry for the heuristicResultMissing Glean metric.
 *
 * This metric is recorded when handleNavigation is called.
 * This occurs in the following cases:
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

add_task(async function test_dragText_heuristicResultMissing() {
  const stub = sinon
    .stub(UrlbarUtils, "getHeuristicResultFor")
    .rejects(new Error("Initialization failed"));

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "data:text/html,<p id='text'>Drag this text</p>" },
    async browser => {
      let draggedText = await SpecialPowers.spawn(
        browser,
        [],
        () => content.document.getElementById("text").textContent
      );

      let dataTransfer = new DataTransfer();
      dataTransfer.setData("text/plain", draggedText);

      let input = gURLBar.inputField;

      let browserLoadedPromise = BrowserTestUtils.browserLoaded(browser);

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

      await browserLoadedPromise;

      let value = Glean.urlbar.heuristicResultMissing.testGetValue();

      Assert.deepEqual(
        value,
        { numerator: 1, denominator: 1 },
        "Should have recorded a numerator and denominator hit for dragText"
      );
    }
  );
  stub.restore();
  Services.fog.testResetFOG();
});

add_task(async function test_enterKey_heuristicResultMissing() {
  const stub = sinon
    .stub(UrlbarUtils, "getHeuristicResultFor")
    .rejects(new Error("Initialization failed"));

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      gURLBar.focus();
      gURLBar.value = "testEnter";

      let browserLoadedPromise = BrowserTestUtils.browserLoaded(browser);

      EventUtils.synthesizeKey("KEY_Enter");

      await browserLoadedPromise;

      let value = Glean.urlbar.heuristicResultMissing.testGetValue();

      Assert.deepEqual(
        value,
        { numerator: 1, denominator: 1 },
        "Should have recorded a numerator and denominator hit for enterKey"
      );
    }
  );

  stub.restore();
  Services.fog.testResetFOG();
});

add_task(async function test_oneOffSearch_heuristicResultMissing() {
  const stub = sinon
    .stub(UrlbarUtils, "getHeuristicResultFor")
    .rejects(new Error("Initialization failed"));
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      gURLBar.focus();
      gURLBar.value = "@test";

      EventUtils.synthesizeKey("KEY_Enter");

      await BrowserTestUtils.browserLoaded(browser);

      let value = Glean.urlbar.heuristicResultMissing.testGetValue();

      Assert.deepEqual(
        value,
        { numerator: 1, denominator: 1 },
        "Should have recorded a numerator and denominator hit for oneOffSearch"
      );
    }
  );

  stub.restore();
  Services.fog.testResetFOG();
});
