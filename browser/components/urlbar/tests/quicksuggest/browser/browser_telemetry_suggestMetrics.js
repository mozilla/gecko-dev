/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const lazy = {};

const EXPECTED_INGEST_LABELS = [
  // Remote settings `type` field values for the default providers
  "data",
  "amo-suggestions",
  "yelp-suggestions",
  "mdn-suggestions",
  // The Suggest component always downloads these remote settings types
  "icon",
  "configuration",
];

const EXPECTED_QUERY_LABELS = [
  // names for the default suggest providers
  "amp",
  "wikipedia",
  "yelp",
  "mdn",
];

const REMOTE_SETTINGS_RECORDS = [
  {
    type: "data",
    attachment: [QuickSuggestTestUtils.ampRemoteSettings()],
  },
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.suggest.enabled", false]],
  });

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: REMOTE_SETTINGS_RECORDS,
    prefs: [["suggest.quicksuggest.sponsored", true]],
  });
});

async function getQueryMetricsLabels() {
  const suggestionTypes = await QuickSuggest.rustBackend
    ._test_enabledSuggestionTypes;
  return suggestionTypes.map(t => t.type.toLowerCase());
}

// Ingest should not update the ingestTime and ingestDownloadTime metrics when
// no new or updated records are ingested.
add_task(async function ingest_unchanged() {
  const oldIngestTimeValues = Object.fromEntries(
    EXPECTED_INGEST_LABELS.map(label => [
      label,
      Glean.suggest.ingestTime[label].testGetValue(),
    ])
  );
  const oldIngestDownloadTimeValues = Object.fromEntries(
    EXPECTED_INGEST_LABELS.map(label => [
      label,
      Glean.suggest.ingestDownloadTime[label].testGetValue(),
    ])
  );
  await QuickSuggestTestUtils.forceSync();
  const newIngestTimeValues = Object.fromEntries(
    EXPECTED_INGEST_LABELS.map(label => [
      label,
      Glean.suggest.ingestTime[label].testGetValue(),
    ])
  );
  const newIngestDownloadTimeValues = Object.fromEntries(
    EXPECTED_INGEST_LABELS.map(label => [
      label,
      Glean.suggest.ingestDownloadTime[label].testGetValue(),
    ])
  );
  for (let label of EXPECTED_INGEST_LABELS) {
    checkLabeledTimingDistributionMetricUnchanged(
      "suggest.ingestTime",
      label,
      oldIngestTimeValues,
      newIngestTimeValues
    );
    checkLabeledTimingDistributionMetricUnchanged(
      "suggest.ingestDownloadTime",
      label,
      oldIngestDownloadTimeValues,
      newIngestDownloadTimeValues
    );
  }
});

// Ingest should update the ingestTime and ingestDownloadTime metrics when new
// or updated records are ingested.
add_task(async function ingest_changed() {
  const oldIngestTimeValues = Object.fromEntries(
    EXPECTED_INGEST_LABELS.map(label => [
      label,
      Glean.suggest.ingestTime[label].testGetValue(),
    ])
  );
  const oldIngestDownloadTimeValues = Object.fromEntries(
    EXPECTED_INGEST_LABELS.map(label => [
      label,
      Glean.suggest.ingestDownloadTime[label].testGetValue(),
    ])
  );

  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "data",
      attachment: [
        QuickSuggestTestUtils.ampRemoteSettings({
          keywords: ["a new keyword"],
        }),
      ],
    },
  ]);

  const newIngestTimeValues = Object.fromEntries(
    EXPECTED_INGEST_LABELS.map(label => [
      label,
      Glean.suggest.ingestTime[label].testGetValue(),
    ])
  );
  const newIngestDownloadTimeValues = Object.fromEntries(
    EXPECTED_INGEST_LABELS.map(label => [
      label,
      Glean.suggest.ingestDownloadTime[label].testGetValue(),
    ])
  );
  checkLabeledTimingDistributionMetricIncreased(
    "suggest.ingestTime",
    "data",
    oldIngestTimeValues,
    newIngestTimeValues
  );
  checkLabeledTimingDistributionMetricIncreased(
    "suggest.ingestDownloadTime",
    "data",
    oldIngestDownloadTimeValues,
    newIngestDownloadTimeValues
  );

  for (let label of EXPECTED_INGEST_LABELS.filter(l => l != "data")) {
    checkLabeledTimingDistributionMetricUnchanged(
      "suggest.ingestTime",
      label,
      oldIngestTimeValues,
      newIngestTimeValues
    );
    checkLabeledTimingDistributionMetricUnchanged(
      "suggest.ingestDownloadTime",
      label,
      oldIngestDownloadTimeValues,
      newIngestDownloadTimeValues
    );
  }

  // Reset the RS data for later tasks.
  await QuickSuggestTestUtils.setRemoteSettingsRecords(REMOTE_SETTINGS_RECORDS);
});

// Queries should update the queryTime metric
add_task(async function query() {
  const oldValues = Object.fromEntries(
    EXPECTED_QUERY_LABELS.map(label => [
      label,
      Glean.suggest.queryTime[label].testGetValue(),
    ])
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "amp",
  });
  await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    index: 1,
    isSponsored: true,
    url: "https://example.com/amp",
  });
  const newValues = Object.fromEntries(
    EXPECTED_QUERY_LABELS.map(label => [
      label,
      Glean.suggest.queryTime[label].testGetValue(),
    ])
  );
  for (let label of EXPECTED_QUERY_LABELS) {
    checkLabeledTimingDistributionMetricIncreased(
      "suggest.queryTime",
      label,
      oldValues,
      newValues
    );
  }
});

function checkLabeledTimingDistributionMetricUnchanged(
  name,
  label,
  oldValues,
  newValues
) {
  Assert.deepEqual(
    oldValues[label],
    newValues[label],
    `The new value for ${name}[${label}] should be the same as the old value`
  );
}

function checkLabeledTimingDistributionMetricIncreased(
  name,
  label,
  oldValues,
  newValues
) {
  Assert.ok(
    newValues[label],
    `The new value for ${name}[${label}] should be non-null`
  );
  Assert.equal(
    typeof newValues[label].count,
    "number",
    `new count for ${name}[${label}] should be a number`
  );
  const oldCount = oldValues[label]?.count ?? 0;
  const newCount = newValues[label].count;
  Assert.greater(
    newCount,
    oldCount,
    `The sample count for ${name}[${label}] should have increased`
  );
}
