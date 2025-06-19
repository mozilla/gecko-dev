/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "l10n", () => {
  return new Localization(["browser/browser.ftl"], true);
});

const TESTS = [
  // Ensure regular calculator results are correctly displayed.
  {
    formula: "8 * 8",
    formattedResult: "64",
    l10nId: "urlbar-result-action-calculator-result-3",
  },
  {
    formula: "10^6",
    formattedResult: "1000000",
    l10nId: "urlbar-result-action-calculator-result-3",
  },
  // Ensure undefined results are correctly displayed.
  {
    formula: "5/0",
    formattedResult: "undefined",
    l10nId: "urlbar-result-action-undefined-calculator-result",
  },
  // Ensure scientific notation results are correctly displayed when
  // below minimum threshold.
  {
    formula: "3/30^12",
    formattedResult: "5.64502927e-18",
    l10nId: "urlbar-result-action-calculator-result-scientific-notation",
  },
  {
    formula: "1000000000 + 2",
    formattedResult: "1000000002",
    l10nId: "urlbar-result-action-calculator-result-3",
  },
  // Ensure scientific notation results are correctly displayed when
  // above maximum threshold.
  {
    formula: "44^8",
    formattedResult: "1.40482236e13",
    l10nId: "urlbar-result-action-calculator-result-scientific-notation",
  },
  // Ensure maximum decimal places rule is followed for repeating decimals.
  {
    formula: "1/3",
    formattedResult: "0.333333333",
    l10nId: "urlbar-result-action-calculator-result-decimal",
  },
  // Ensure negative calculator results are correctly displayed.
  {
    formula: "-50000000 + 1",
    formattedResult: "-49999999",
    l10nId: "urlbar-result-action-calculator-result-3",
  },
  {
    formula: "-1/3",
    formattedResult: "-0.333333333",
    l10nId: "urlbar-result-action-calculator-result-decimal",
  },
  {
    formula: "-10^13",
    formattedResult: "-1.0e13",
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
    const { formula, formattedResult, l10nId } = test;

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: formula,
    });

    let res = (await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1))
      .result;
    Assert.equal(res.type, UrlbarUtils.RESULT_TYPE.DYNAMIC);
    Assert.equal(res.payload.input, formula);

    EventUtils.synthesizeKey("KEY_ArrowDown");

    let result = await lazy.l10n.formatValue(l10nId, {
      result: res.payload.value,
    });

    if (result.startsWith("=")) {
      result = result.slice(1).trim();
    }

    info("Check that the displayed calculator result is correct");
    Assert.equal(formattedResult, result);

    // Ensure the localized result which is displayed is what gets copied to clipboard.
    await SimpleTest.promiseClipboardChange(result, () => {
      EventUtils.synthesizeKey("KEY_Enter");
    });
  }
});
