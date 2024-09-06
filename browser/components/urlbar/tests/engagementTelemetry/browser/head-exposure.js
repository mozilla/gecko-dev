/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function doExposureTest({
  prefs,
  query,
  expectedResultTypes,
  shouldBeShown,
  trigger = doBlur,
}) {
  expectedResultTypes = expectedResultTypes
    .map(t => suggestResultType(t))
    .sort();

  const cleanupQuickSuggest = await ensureQuickSuggestInit({ prefs });

  await doTest(async () => {
    await openPopup(query);

    for (let type of expectedResultTypes) {
      let result = await getResultByType(type);
      Assert.equal(
        !!result,
        shouldBeShown,
        "The result should be in the view iff it should be shown"
      );
    }

    await trigger();

    let results = expectedResultTypes.join(",");
    assertExposureTelemetry(results ? [{ results }] : []);
  });

  await cleanupQuickSuggest();
}

async function getResultByType(type) {
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    const detail = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    const telemetryType = UrlbarUtils.searchEngagementTelemetryType(
      detail.result
    );
    if (telemetryType === type) {
      return detail.result;
    }
  }
  return null;
}

function suggestResultType(typeWithoutSource) {
  let source = UrlbarPrefs.get("quickSuggestRustEnabled") ? "rust" : "rs";
  return `${source}_${typeWithoutSource}`;
}
