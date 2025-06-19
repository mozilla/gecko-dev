/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};

const TESTS = [
  // Ensure regular calculator results are correctly displayed.
  {
    formula: "8 * 8",
    formattedResult: "64",
  },
  {
    formula: "10^6",
    formattedResult: "1000000",
  },
  // Ensure undefined results are correctly displayed.
  {
    formula: "5/0",
    formattedResult: "undefined",
  },
  // Ensure scientific notation results are correctly displayed when
  // below minimum threshold.
  {
    formula: "3/30^12",
    formattedResult: "5.64502927e-18",
  },
  {
    formula: "1000000000 + 2",
    formattedResult: "1000000002",
  },
  // Ensure scientific notation results are correctly displayed when
  // above maximum threshold.
  {
    formula: "44^8",
    formattedResult: "1.40482236e13",
  },
  // Ensure maximum decimal places rule is followed for repeating decimals.
  {
    formula: "1/3",
    formattedResult: "0.333333333",
  },
  // Ensure negative calculator results are correctly displayed.
  {
    formula: "-50000000 + 1",
    formattedResult: "-49999999",
  },
  {
    formula: "-1/3",
    formattedResult: "-0.333333333",
  },
  {
    formula: "-10^13",
    formattedResult: "-1.0e13",
  },
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.calculator", true]],
  });
});

add_task(async function test_calculator() {
  for (let test of TESTS) {
    const { formula, formattedResult } = test;

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: formula,
    });

    let res = (await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1))
      .result;
    Assert.equal(res.type, UrlbarUtils.RESULT_TYPE.DYNAMIC);
    Assert.equal(res.payload.input, formula);

    EventUtils.synthesizeKey("KEY_ArrowDown");

    info("Check that the displayed calculator result is correct");
    Assert.equal(formattedResult, res.payload.value);

    // Ensure the localized result which is displayed is what gets copied to clipboard.
    await SimpleTest.promiseClipboardChange(formattedResult, () => {
      EventUtils.synthesizeKey("KEY_Enter");
    });
  }
});
