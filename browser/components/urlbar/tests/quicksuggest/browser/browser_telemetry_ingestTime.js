/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the `urlbar.quick_suggest_ingest_time` Glean probe.

"use strict";

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

// A successful ingest should update the timing distribution.
add_task(async function successfulIngest() {
  let oldValue = Glean.urlbar.quickSuggestIngestTime.testGetValue();
  info("Got old value: " + JSON.stringify(oldValue));

  await QuickSuggestTestUtils.forceSync();

  let newValue = Glean.urlbar.quickSuggestIngestTime.testGetValue();
  info("Got new value: " + JSON.stringify(oldValue));

  Assert.ok(oldValue, "The old value should be non-null");
  Assert.ok(newValue, "The new value should be non-null");

  Assert.equal(
    typeof oldValue.count,
    "number",
    "oldValue.count should be a number"
  );
  Assert.equal(
    typeof newValue.count,
    "number",
    "newValue.count should be a number"
  );

  let enabledCount =
    QuickSuggest.rustBackend._test_enabledSuggestionTypes.length;
  Assert.equal(
    newValue.count,
    oldValue.count + enabledCount,
    "One new value per enabled suggestion type should have been recorded after ingest"
  );
});
