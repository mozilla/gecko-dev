/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for dynamicSuggestions.

const REMOTE_SETTINGS_DATA = [
  {
    type: "dynamic-suggestions",
    suggestion_type: "basic",
    attachment: [
      {
        keywords: ["basic"],
        data: {
          result: {
            payload: {
              title: "basic",
              url: "https://example.com/basic",
              icon: "https://example.com/basic.svg",
            },
          },
        },
      },
    ],
  },
  {
    type: "dynamic-suggestions",
    suggestion_type: "shouldShowUrl",
    attachment: [
      {
        keywords: ["shouldshowurl"],
        data: {
          result: {
            payload: {
              title: "shouldShowUrl",
              url: "https://example.com/shouldShowUrl",
              icon: "https://example.com/shouldShowUrl.svg",
              shouldShowUrl: true,
            },
          },
        },
      },
    ],
  },
  {
    type: "dynamic-suggestions",
    suggestion_type: "isBlockable",
    attachment: [
      {
        keywords: ["isblockable"],
        dismissal_key: "isblockable-dismissal-key",
        data: {
          result: {
            payload: {
              title: "isBlockable",
              url: "https://example.com/isBlockable",
              icon: "https://example.com/isBlockable.svg",
              isBlockable: true,
            },
          },
        },
      },
    ],
  },
  {
    type: "dynamic-suggestions",
    suggestion_type: "rowLabel",
    attachment: [
      {
        keywords: ["rowlabel"],
        data: {
          result: {
            rowLabel: {
              id: "urlbar-group-search-suggestions",
              args: { engine: "Test" },
            },
            payload: {
              url: "https://example.com/rowLabel",
            },
          },
        },
      },
    ],
  },
];

add_setup(async function () {
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: REMOTE_SETTINGS_DATA,
  });
  await UrlbarTestUtils.initNimbusFeature({
    quickSuggestDynamicSuggestionTypes:
      "basic,shouldShowUrl,isBlockable,rowLabel",
  });

  // Wait until dynamic suggestion is available.
  await BrowserTestUtils.waitForCondition(async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "basic",
    });
    const result = UrlbarTestUtils.getResultCount(window) == 2;
    await UrlbarTestUtils.promisePopupClose(window);
    return result;
  });
});

add_task(async function basic() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    info("Open urlbar with keyword");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "basic",
    });
    Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

    info("Check the result");
    const { element, result } = await UrlbarTestUtils.getDetailsOfResultAt(
      window,
      1
    );
    Assert.equal(result.providerName, UrlbarProviderQuickSuggest.name);
    Assert.equal(result.payload.provider, "Dynamic");
    assertUI(
      element.row,
      REMOTE_SETTINGS_DATA[0].attachment[0].data.result.payload
    );

    info("Activate this item");
    const onLoad = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      result.payload.url
    );
    EventUtils.synthesizeMouseAtCenter(element.row, {});
    await onLoad;
    Assert.ok(true, "Expected page is loaded");
  });

  await PlacesUtils.history.clear();
});

add_task(async function basic_learn_more() {
  info("Open urlbar with keyword");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "basic",
  });

  info("Selecting Learn more item from the result menu");
  let tabOpenPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    QuickSuggest.HELP_URL
  );
  await UrlbarTestUtils.openResultMenuAndClickItem(window, "help", {
    resultIndex: 1,
  });
  await tabOpenPromise;
  gBrowser.removeCurrentTab();
});

add_task(async function basic_manage() {
  await doManageTest({ index: 1, input: "basic" });
});

add_task(async function shouldShowUrl() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    info("Open urlbar with keyword");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "shouldshowurl",
    });
    Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

    info("Check the result");
    const { element, result } = await UrlbarTestUtils.getDetailsOfResultAt(
      window,
      1
    );
    Assert.equal(result.providerName, UrlbarProviderQuickSuggest.name);
    Assert.equal(result.payload.provider, "Dynamic");
    assertUI(
      element.row,
      REMOTE_SETTINGS_DATA[1].attachment[0].data.result.payload
    );
  });
});

add_task(async function isBlockable() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    info("Open urlbar with keyword");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "isblockable",
    });
    Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

    info("Check the result");
    const { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
    Assert.equal(result.providerName, UrlbarProviderQuickSuggest.name);
    Assert.equal(result.payload.provider, "Dynamic");

    info("Dismiss this item");
    let dismissalPromise = TestUtils.topicObserved(
      "quicksuggest-dismissals-changed"
    );
    await UrlbarTestUtils.openResultMenuAndClickItem(window, "dismiss", {
      resultIndex: 1,
    });
    await dismissalPromise;

    Assert.ok(
      UrlbarTestUtils.isPopupOpen(window),
      "View remains open after blocking result"
    );
    Assert.ok(
      await QuickSuggest.isResultDismissed(result),
      "Result should be dismissed"
    );

    await UrlbarTestUtils.promisePopupClose(window);
    await QuickSuggest.clearDismissedSuggestions();
  });
});

add_task(async function rowLabel() {
  info("Open urlbar with keyword");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "rowlabel",
  });
  Assert.equal(UrlbarTestUtils.getResultCount(window), 2);

  info("Check the row label");
  const { element } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(element.row.getAttribute("label"), "Test suggestions");
});

function assertUI(row, payload) {
  const titleElement = row.querySelector(".urlbarView-title");
  Assert.equal(titleElement.textContent, payload.title);

  const faviconElement = row.querySelector(".urlbarView-favicon");
  Assert.equal(faviconElement.src, payload.icon);

  const urlElement = row.querySelector(".urlbarView-url");
  const displayUrl = payload.shouldShowUrl
    ? payload.url.replace(/^https:\/\//, "")
    : "";
  Assert.equal(urlElement.textContent, displayUrl);
}
