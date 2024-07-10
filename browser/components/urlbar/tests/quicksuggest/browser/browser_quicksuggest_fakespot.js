/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for Fakespot suggestions.

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

const DUMMY_RESULT = {
  source: "rust",
  provider: "Fakespot",
  url: "https://example.com/maybe-good-item",
  title: "Maybe Good Item",
  rating: "4.8",
  totalReviews: "1234567",
  fakespotGrade: "A",
};

add_setup(async function () {
  const sandbox = lazy.sinon.createSandbox();
  sandbox
    .stub(lazy.QuickSuggest.rustBackend, "query")
    .callsFake(async _searchString => [DUMMY_RESULT]);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.quicksuggest.rustEnabled", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.fakespot.featureGate", true],
      ["browser.urlbar.suggest.fakespot", true],
    ],
  });

  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    sandbox.restore();
  });
});

add_task(async function basic() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "Maybe",
    });
    Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

    const { element, result } = await UrlbarTestUtils.getDetailsOfResultAt(
      window,
      1
    );
    Assert.equal(
      result.providerName,
      UrlbarProviderQuickSuggest.name,
      "The result should be from the expected provider"
    );
    Assert.equal(result.payload.provider, "Fakespot");

    const onLoad = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      "https://example.com/maybe-good-item"
    );
    EventUtils.synthesizeMouseAtCenter(element.row, {});
    await onLoad;
    Assert.ok(true, "Expected page is loaded");
  });

  await PlacesUtils.history.clear();
});

add_task(async function disable() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.fakespot.featureGate", false]],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  Assert.equal(UrlbarTestUtils.getResultCount(window), 1);

  const { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.notEqual(result.telemetryType, "fakespot");

  await SpecialPowers.popPrefEnv();
});
