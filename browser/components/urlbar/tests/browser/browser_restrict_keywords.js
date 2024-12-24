/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks that searching for "@" provides restrict keywords
// (@bookmarks, @history, @tabs, @actions) and verifies that selecting one of
// these keywords enters the appropriate search mode.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.sys.mjs",
});

const RESTRICT_TOKENS = [
  UrlbarTokenizer.RESTRICT.HISTORY,
  UrlbarTokenizer.RESTRICT.BOOKMARK,
  UrlbarTokenizer.RESTRICT.OPENPAGE,
  UrlbarTokenizer.RESTRICT.ACTION,
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.searchRestrictKeywords.featureGate", true]],
  });

  registerCleanupFunction(async function () {
    sinon.restore();
  });
});

async function getRestrictKeywordResult(window, restrictToken) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@",
  });

  let restrictResult;
  let resultIndex;
  let resultCount = await UrlbarTestUtils.getResultCount(window);

  for (let index = 0; !restrictResult && index < resultCount; index++) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    if (details.result.type != UrlbarUtils.RESULT_TYPE.RESTRICT) {
      continue;
    }
    let l10nRestrictKeywords = details.result.payload.l10nRestrictKeywords;
    let localSearchMode = l10nRestrictKeywords[0].toLowerCase();
    let keywords = l10nRestrictKeywords
      .map(keyword => `@${keyword.toLowerCase()}`)
      .join(", ");
    let symbol = details.result.payload.keyword;

    if (symbol == restrictToken) {
      Assert.equal(
        details.displayed.title,
        `${keywords} - Search ${localSearchMode}`,
        "The result's title is set correctly."
      );

      restrictResult = await UrlbarTestUtils.waitForAutocompleteResultAt(
        window,
        index
      );
      resultIndex = index;
    }
  }

  return { restrictResult, resultIndex };
}

async function exitSearchModeAndClosePanel() {
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );
}

async function assertRestrictKeywordResult(window, restrictToken) {
  Services.telemetry.clearScalars();
  let { restrictResult, resultIndex } = await getRestrictKeywordResult(
    window,
    restrictToken
  );

  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  EventUtils.synthesizeMouseAtCenter(restrictResult, {});
  await searchPromise;

  let searchMode = UrlbarUtils.searchModeForToken(
    restrictResult.result.payload.keyword
  );
  await UrlbarTestUtils.assertSearchMode(window, {
    ...searchMode,
    entry: "keywordoffer",
    restrictType: "keyword",
  });

  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  let l10nRestrictKeywords = restrictResult.result.payload.l10nRestrictKeywords;
  let englishLocalSearchMode =
    l10nRestrictKeywords.length > 1
      ? l10nRestrictKeywords[1].toLowerCase()
      : l10nRestrictKeywords[0].toLowerCase();
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    `urlbar.picked.restrict_keyword_${englishLocalSearchMode}`,
    resultIndex,
    1
  );

  await exitSearchModeAndClosePanel();
}

add_task(async function test_search_restrict_keyword_results() {
  for (const restrictToken of RESTRICT_TOKENS) {
    await assertRestrictKeywordResult(window, restrictToken);
  }
});

add_task(async function test_search_restrict_keyword_results_es_en_locales() {
  let spanishEnglishKeywords = new Map([
    ["*", ["Marcadores", "Bookmarks"]],
    ["%", ["Pesta\xF1as", "Tabs"]],
    ["^", ["Historial", "History"]],
    [">", ["Acciones", "Actions"]],
  ]);

  let tokenizerStub = sinon.stub(UrlbarTokenizer, "getL10nRestrictKeywords");
  tokenizerStub.resolves(spanishEnglishKeywords);

  for (const restrictToken of RESTRICT_TOKENS) {
    await assertRestrictKeywordResult(window, restrictToken);
  }
});
