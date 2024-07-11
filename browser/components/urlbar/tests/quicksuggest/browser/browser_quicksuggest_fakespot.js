/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for Fakespot suggestions.

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

// TODO: Replace with proper remote settings data once the Fakespot Rust feature
// is vendored in to mozilla-central.
const DUMMY_RESULT = {
  source: "rust",
  provider: "Fakespot",
  url: "https://example.com/maybe-good-item",
  title: "Maybe Good Item",
  rating: 4.8,
  totalReviews: 1234567,
  fakespotGrade: "A",
};

const HELP_URL =
  Services.urlFormatter.formatURLPref("app.support.baseURL") +
  "awesome-bar-result-menu";

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

// Tests the "Show less frequently" result menu command.
add_task(async function resultMenu_show_less_frequently() {
  info(
    "Test for no fakespotMinKeywordLength and no fakespotShowLessFrequentlyCap"
  );
  await doShowLessFrequently({
    minKeywordLength: 0,
    frequentlyCap: 0,
    testData: [
      {
        input: "maybe",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "maybe",
        expected: {
          hasSuggestion: false,
        },
      },
      {
        input: "maybe g",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "maybe g",
        expected: {
          hasSuggestion: false,
        },
      },
      {
        input: "maybe go",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "maybe go",
        expected: {
          hasSuggestion: false,
        },
      },
    ],
  });

  info("Test whether fakespotShowLessFrequentlyCap can work");
  await doShowLessFrequently({
    minKeywordLength: 0,
    frequentlyCap: 2,
    testData: [
      {
        input: "maybe g",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "maybe go",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "maybe go",
        expected: {
          hasSuggestion: false,
        },
      },
      {
        input: "maybe goo",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: false,
        },
      },
      {
        input: "maybe good",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: false,
        },
      },
    ],
  });

  info(
    "Test whether local fakespot.minKeywordLength pref can override nimbus variable fakespotMinKeywordLength"
  );
  await doShowLessFrequently({
    minKeywordLength: 8,
    frequentlyCap: 0,
    testData: [
      {
        input: "maybe g",
        expected: {
          hasSuggestion: false,
        },
      },
      {
        input: "maybe go",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "maybe goo",
        expected: {
          hasSuggestion: true,
          hasShowLessItem: true,
        },
      },
      {
        input: "maybe goo",
        expected: {
          hasSuggestion: false,
        },
      },
    ],
  });
});

async function doShowLessFrequently({
  minKeywordLength,
  frequentlyCap,
  testData,
}) {
  UrlbarPrefs.clear("fakespot.showLessFrequentlyCount");
  UrlbarPrefs.clear("fakespot.minKeywordLength");

  let cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature({
    fakespotMinKeywordLength: minKeywordLength,
    fakespotShowLessFrequentlyCap: frequentlyCap,
  });

  for (let { input, expected } of testData) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: input,
    });

    if (expected.hasSuggestion) {
      let resultIndex = 1;
      let details = await UrlbarTestUtils.getDetailsOfResultAt(
        window,
        resultIndex
      );
      Assert.equal(details.result.payload.provider, "Fakespot");

      if (expected.hasShowLessItem) {
        // Click the command.
        let previousShowLessFrequentlyCount = UrlbarPrefs.get(
          "fakespot.showLessFrequentlyCount"
        );
        await UrlbarTestUtils.openResultMenuAndClickItem(
          window,
          "show_less_frequently",
          { resultIndex, openByMouse: true }
        );

        Assert.equal(
          UrlbarPrefs.get("fakespot.showLessFrequentlyCount"),
          previousShowLessFrequentlyCount + 1
        );
        Assert.equal(
          UrlbarPrefs.get("fakespot.minKeywordLength"),
          input.length + 1
        );
      } else {
        let menuitem = await UrlbarTestUtils.openResultMenuAndGetItem({
          window,
          command: "show_less_frequently",
          resultIndex: 1,
          openByMouse: true,
        });
        Assert.ok(!menuitem);
      }
    } else {
      // Fakespot suggestion should not be shown.
      for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
        let details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
        Assert.notEqual(details.result.payload.provider, "Fakespot");
      }
    }

    await UrlbarTestUtils.promisePopupClose(window);
  }

  await cleanUpNimbus();
  UrlbarPrefs.clear("fakespot.showLessFrequentlyCount");
  UrlbarPrefs.clear("fakespot.minKeywordLength");
}

// Tests the "Not relevant" result menu dismissal command.
add_task(async function resultMenu_not_relevant() {
  await doDismiss({
    menu: "not_relevant",
    assert: resuilt => {
      Assert.ok(
        QuickSuggest.blockedSuggestions.has(resuilt.payload.url),
        "The URL should be register as blocked"
      );
    },
  });

  await QuickSuggest.blockedSuggestions.clear();
});

// Tests the "Not interested" result menu dismissal command.
add_task(async function resultMenu_not_interested() {
  await doDismiss({
    menu: "not_interested",
    assert: () => {
      Assert.ok(!UrlbarPrefs.get("suggest.fakespot"));
    },
  });

  UrlbarPrefs.clear("suggest.fakespot");
});

async function doDismiss({ menu, assert }) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "maybe",
  });

  let resultCount = UrlbarTestUtils.getResultCount(window);
  let resultIndex = 1;
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(details.result.payload.provider, "Fakespot");
  let result = details.result;

  // Click the command.
  await UrlbarTestUtils.openResultMenuAndClickItem(
    window,
    ["[data-l10n-id=firefox-suggest-command-manage-fakespot]", menu],
    {
      resultIndex,
      openByMouse: true,
    }
  );

  // The row should be a tip now.
  Assert.ok(gURLBar.view.isOpen, "The view should remain open after dismissal");
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultCount,
    "The result count should not haved changed after dismissal"
  );
  details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(
    details.type,
    UrlbarUtils.RESULT_TYPE.TIP,
    "Row should be a tip after dismissal"
  );
  Assert.equal(
    details.result.payload.type,
    "dismissalAcknowledgment",
    "Tip type should be dismissalAcknowledgment"
  );
  Assert.ok(
    !details.element.row.hasAttribute("feedback-acknowledgment"),
    "Row should not have feedback acknowledgment after dismissal"
  );

  // Get the dismissal acknowledgment's "Got it" button and click it.
  let gotItButton = UrlbarTestUtils.getButtonForResultIndex(
    window,
    "0",
    resultIndex
  );
  Assert.ok(gotItButton, "Row should have a 'Got it' button");
  EventUtils.synthesizeMouseAtCenter(gotItButton, {}, window);

  // The view should remain open and the tip row should be gone.
  Assert.ok(
    gURLBar.view.isOpen,
    "The view should remain open clicking the 'Got it' button"
  );
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultCount - 1,
    "The result count should be one less after clicking 'Got it' button"
  );

  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.ok(
      details.type != UrlbarUtils.RESULT_TYPE.TIP &&
        details.result.payload.provider !== "Fakespot",
      "Tip result and Fakespot result should not be present"
    );
  }

  assert(result);

  await UrlbarTestUtils.promisePopupClose(window);

  // Check that the result should not be shown anymore.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "maybe",
  });

  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.ok(
      details.result.payload.provider !== "Fakespot",
      "Fakespot result should not be present"
    );
  }

  await UrlbarTestUtils.promisePopupClose(window);
}

// Tests the "Manage" result menu.
add_task(async function resultMenu_manage() {
  await doManageTest({ input: "maybe", index: 1 });
});

// Tests the "Learn more" (a.k.a. "Help") result menu command.
add_task(async function resultMenu_help() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "maybe",
  });

  let resultIndex = 1;
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(details.result.payload.provider, "Fakespot");

  let loadPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  await UrlbarTestUtils.openResultMenuAndClickItem(window, "help", {
    resultIndex,
    openByMouse: true,
  });

  info("Waiting for the help page to load");
  await loadPromise;
  await TestUtils.waitForTick();
  Assert.equal(
    gBrowser.currentURI.spec,
    HELP_URL,
    "The loaded URL should be the help URL"
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
