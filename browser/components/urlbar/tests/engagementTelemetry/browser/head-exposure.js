/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function doExposureTest({
  prefs,
  queries,
  expectedEvents,
  trigger = doBlur,
}) {
  const cleanupQuickSuggest = await ensureQuickSuggestInit({ prefs });

  await doTest(async () => {
    for (let {
      query,
      expectedVisible = [],
      expectedNotVisible = [],
    } of queries) {
      info("doExposureTest performing query: " + JSON.stringify(query));
      await openPopup(query);

      for (let type of expectedVisible) {
        let row = await getRowByType(suggestResultType(type));
        Assert.ok(!!row, "The result should be in the view: " + type);
        Assert.ok(
          BrowserTestUtils.isVisible(row),
          "The result's row should be visible: " + type
        );
      }
      for (let type of expectedNotVisible) {
        Assert.ok(
          !(await getRowByType(suggestResultType(type))),
          "The result should not be in the view: " + type
        );
      }
    }

    await trigger();

    assertExposureTelemetry(
      expectedEvents.map(e => ({
        ...e,
        results: e.results
          .split(",")
          .map(r => suggestResultType(r))
          .join(","),
      }))
    );
  });

  await cleanupQuickSuggest();
}

async function getRowByType(type) {
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    const detail = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    const telemetryType = UrlbarUtils.searchEngagementTelemetryType(
      detail.result
    );
    if (telemetryType === type) {
      return detail.element.row;
    }
  }
  return null;
}

function suggestResultType(typeWithoutSource) {
  let source = UrlbarPrefs.get("quickSuggestRustEnabled") ? "rust" : "rs";
  return `${source}_${typeWithoutSource}`;
}
