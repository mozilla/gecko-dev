/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

// Tests autofill functionality for restrict keywords (@tabs, @bookmarks,
// @history, @actions) in localized language and in english. The autofill
// is tested by typing the full or partial keyword to enter search mode.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.sys.mjs",
});

let gFluentStrings = new Localization(["browser/browser.ftl"]);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.searchRestrictKeywords.featureGate", true]],
  });

  let italianEnglishKeywords = new Map([
    ["^", ["Cronologia", "History"]],
    ["*", ["Segnalibri", "Bookmarks"]],
    ["%", ["Schede", "Tabs"]],
    [">", ["Azioni", "Actions"]],
  ]);

  let tokenizerStub = sinon.stub(UrlbarTokenizer, "getL10nRestrictKeywords");
  tokenizerStub.resolves(italianEnglishKeywords);

  registerCleanupFunction(async function () {
    sinon.restore();
  });
});

async function assertAutofill(
  searchString,
  searchMode,
  entry,
  userEventAction
) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: searchString,
  });

  if (userEventAction) {
    userEventAction();
  }
  await UrlbarTestUtils.assertSearchMode(window, {
    ...searchMode,
    entry,
    restrictType: "keyword",
  });

  await UrlbarTestUtils.exitSearchMode(window);
}

async function getSearchModeKeywords() {
  let searchModeKeys = [
    "urlbar-search-mode-bookmarks",
    "urlbar-search-mode-tabs",
    "urlbar-search-mode-history",
    "urlbar-search-mode-actions",
  ];

  let [bookmarks, tabs, history, actions] = await Promise.all(
    searchModeKeys.map(key =>
      gFluentStrings.formatValue(key).then(str => `@${str.toLowerCase()}`)
    )
  );

  return [bookmarks, tabs, history, actions];
}

add_task(async function test_autofill_enters_search_mode_it_en_locales() {
  let [bookmarks, tabs, history, actions] = await getSearchModeKeywords();

  const keywordsToToken = new Map([
    [["@cronologia", history], UrlbarTokenizer.RESTRICT.HISTORY],
    [["@segnalibri", bookmarks], UrlbarTokenizer.RESTRICT.BOOKMARK],
    [["@schede", tabs], UrlbarTokenizer.RESTRICT.OPENPAGE],
    [["@azioni", actions], UrlbarTokenizer.RESTRICT.ACTION],
  ]);

  for (const [keywords, token] of keywordsToToken) {
    let searchMode = UrlbarUtils.searchModeForToken(token);
    let italianKeyword = `${keywords[0]} `;
    let englishKeyword = `${keywords[1]} `;

    info("Test full italian keyword");
    await assertAutofill(italianKeyword, searchMode, "typed", null);

    info("Test partial italian keyword autofill by pressing right arrow");
    italianKeyword = italianKeyword.slice(0, 3);
    await assertAutofill(italianKeyword, searchMode, "typed", () =>
      EventUtils.synthesizeKey("KEY_ArrowRight")
    );

    info("Test partial italian keyword autofill by pressing enter");
    await assertAutofill(italianKeyword, searchMode, "keywordoffer", () =>
      EventUtils.synthesizeKey("KEY_Enter")
    );

    info("Test full english keyword");
    await assertAutofill(englishKeyword, searchMode, "typed", null);

    info("Test partial english keyword autofill by pressing right arrow");
    englishKeyword = englishKeyword.slice(0, 3);
    await assertAutofill(englishKeyword, searchMode, "typed", () =>
      EventUtils.synthesizeKey("KEY_ArrowRight")
    );

    info("Test partial english keyword autofill by pressing enter");
    await assertAutofill(englishKeyword, searchMode, "keywordoffer", () =>
      EventUtils.synthesizeKey("KEY_Enter")
    );
  }
});
