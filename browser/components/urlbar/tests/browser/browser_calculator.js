/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "l10n", () => {
  return new Localization(["browser/browser.ftl"], true);
});

const TESTS = [
  {
    formula: "8 * 8",
    result: "64",
    l10nId: "urlbar-result-action-calculator-result-2",
  },
  {
    formula: "10^6",
    result: "1000000",
    l10nId: "urlbar-result-action-calculator-result-2",
  },
  {
    formula: "5/0",
    result: "undefined",
    l10nId: "urlbar-result-action-undefined-calculator-result",
  },
  {
    formula: "3/30^12",
    result: "5.64502927e-18",
    l10nId: "urlbar-result-action-calculator-result-scientific-notation",
  },
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.calculator", true]],
  });
});

add_task(async function test_calculator() {
  for (let test of TESTS) {
    const { formula, result, l10nId } = test;

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: formula,
    });

    let res = (await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1))
      .result;
    Assert.equal(res.type, UrlbarUtils.RESULT_TYPE.DYNAMIC);
    Assert.equal(res.payload.input, formula);
    Assert.equal(res.payload.value, result);

    EventUtils.synthesizeKey("KEY_ArrowDown");

    let localizedResult = await lazy.l10n.formatValue(l10nId, {
      result: res.payload.value,
    });

    if (localizedResult.startsWith("=")) {
      localizedResult = localizedResult.slice(1).trim();
    }

    // Ensure the localized result which is displayed is what gets copied to clipboard.
    await SimpleTest.promiseClipboardChange(localizedResult, () => {
      EventUtils.synthesizeKey("KEY_Enter");
    });
  }
});
