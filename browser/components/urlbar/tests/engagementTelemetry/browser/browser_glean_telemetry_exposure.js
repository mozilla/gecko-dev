/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// test for exposure events

"use strict";

// Avoid timeouts in verify mode, especially on Mac
requestLongerTimeout(6);

add_setup(async function () {
  await initExposureTest();
});

add_task(async function engagement() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", suggestResultType("adm_sponsored")],
      ["browser.urlbar.showExposureResults", true],
    ],
    query: "amp",
    trigger: doClick,
    expectedResultTypes: ["adm_sponsored"],
    shouldBeShown: true,
  });
});

add_task(async function abandonment() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", suggestResultType("adm_sponsored")],
      ["browser.urlbar.showExposureResults", true],
    ],
    query: "amp",
    trigger: doBlur,
    expectedResultTypes: ["adm_sponsored"],
    shouldBeShown: true,
  });
});

add_task(async function oneExposureResult_shown_matched() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", suggestResultType("adm_sponsored")],
      ["browser.urlbar.showExposureResults", true],
    ],
    query: "amp",
    expectedResultTypes: ["adm_sponsored"],
    shouldBeShown: true,
  });
});

add_task(async function oneExposureResult_shown_notMatched() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", suggestResultType("adm_sponsored")],
      ["browser.urlbar.showExposureResults", true],
    ],
    query: "wikipedia",
    expectedResultTypes: [],
  });
});

add_task(async function oneExposureResult_hidden_matched() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", suggestResultType("adm_sponsored")],
      ["browser.urlbar.showExposureResults", false],
    ],
    query: "amp",
    expectedResultTypes: ["adm_sponsored"],
    shouldBeShown: false,
  });
});

add_task(async function oneExposureResult_hidden_notMatched() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", suggestResultType("adm_sponsored")],
      ["browser.urlbar.showExposureResults", false],
    ],
    query: "wikipedia",
    expectedResultTypes: [],
  });
});

add_task(async function manyExposureResults_shown_oneMatched_1() {
  await doExposureTest({
    prefs: [
      [
        "browser.urlbar.exposureResults",
        [
          suggestResultType("adm_sponsored"),
          suggestResultType("adm_nonsponsored"),
        ].join(","),
      ],
      ["browser.urlbar.showExposureResults", true],
    ],
    query: "amp",
    expectedResultTypes: ["adm_sponsored"],
    shouldBeShown: true,
  });
});

add_task(async function manyExposureResults_shown_oneMatched_2() {
  await doExposureTest({
    prefs: [
      [
        "browser.urlbar.exposureResults",
        [
          suggestResultType("adm_sponsored"),
          suggestResultType("adm_nonsponsored"),
        ].join(","),
      ],
      ["browser.urlbar.showExposureResults", true],
    ],
    query: "wikipedia",
    expectedResultTypes: ["adm_nonsponsored"],
    shouldBeShown: true,
  });
});

add_task(async function manyExposureResults_shown_manyMatched() {
  await doExposureTest({
    prefs: [
      [
        "browser.urlbar.exposureResults",
        [
          suggestResultType("adm_sponsored"),
          suggestResultType("adm_nonsponsored"),
        ].join(","),
      ],
      ["browser.urlbar.showExposureResults", true],
    ],
    query: "amp and wikipedia",
    // Only one result should be recorded since exposures are shown and at most
    // one Suggest result should be shown.
    expectedResultTypes: ["adm_sponsored"],
    shouldBeShown: true,
  });
});

add_task(async function manyExposureResults_hidden_oneMatched_1() {
  await doExposureTest({
    prefs: [
      [
        "browser.urlbar.exposureResults",
        [
          suggestResultType("adm_sponsored"),
          suggestResultType("adm_nonsponsored"),
        ].join(","),
      ],
      ["browser.urlbar.showExposureResults", false],
    ],
    query: "amp",
    expectedResultTypes: ["adm_sponsored"],
    shouldBeShown: false,
  });
});

add_task(async function manyExposureResults_hidden_oneMatched_2() {
  await doExposureTest({
    prefs: [
      [
        "browser.urlbar.exposureResults",
        [
          suggestResultType("adm_sponsored"),
          suggestResultType("adm_nonsponsored"),
        ].join(","),
      ],
      ["browser.urlbar.showExposureResults", false],
    ],
    query: "wikipedia",
    expectedResultTypes: ["adm_nonsponsored"],
    shouldBeShown: false,
  });
});

add_task(async function manyExposureResults_hidden_manyMatched() {
  await doExposureTest({
    prefs: [
      [
        "browser.urlbar.exposureResults",
        [
          suggestResultType("adm_sponsored"),
          suggestResultType("adm_nonsponsored"),
        ].join(","),
      ],
      ["browser.urlbar.showExposureResults", false],
    ],
    query: "amp and wikipedia",
    // Both results should be recorded since exposures are hidden and there's no
    // limit on the number of hidden-exposure Suggest results.
    expectedResultTypes: ["adm_nonsponsored", "adm_sponsored"],
    shouldBeShown: false,
  });
});

add_task(async function modifyQuery_terminal() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", suggestResultType("adm_sponsored")],
      ["browser.urlbar.showExposureResults", true],
    ],
    // start with a Wikipedia query
    query: "wikipedia",
    trigger: async () => {
      // delete the Wikipedia query
      gURLBar.select();
      EventUtils.synthesizeKey("KEY_Backspace");
      // trigger the AMP suggestion
      await openPopup("amp");
      await doClick();
    },
    expectedResultTypes: ["adm_sponsored"],
    // `shouldBeShown` is checked before `trigger` is called, and the AMP result
    // won't be present then.
    shouldBeShown: false,
  });
});

add_task(async function modifyQuery_nonTerminal() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", suggestResultType("adm_sponsored")],
      ["browser.urlbar.showExposureResults", true],
    ],
    // start with an AMP query
    query: "amp",
    trigger: async () => {
      // delete the AMP query
      gURLBar.select();
      EventUtils.synthesizeKey("KEY_Backspace");
      // trigger the Wikipedia suggestion
      await openPopup("wikipedia");
      await doClick();
    },
    expectedResultTypes: ["adm_sponsored"],
    // `shouldBeShown` is checked before `trigger` is called, and the AMP result
    // will be present then.
    shouldBeShown: true,
  });
});
