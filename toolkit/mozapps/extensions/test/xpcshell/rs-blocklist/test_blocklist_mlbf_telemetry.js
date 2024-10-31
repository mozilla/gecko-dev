/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("extensions.blocklist.useMLBF", true);
Services.prefs.setBoolPref("extensions.blocklist.softblock.enabled", true);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

const { Downloader } = ChromeUtils.importESModule(
  "resource://services-settings/Attachments.sys.mjs"
);

const { TelemetryController } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryController.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const OLDEST_STASH = { stash: { blocked: [], unblocked: [] }, stash_time: 2e6 };
const NEWEST_STASH = { stash: { blocked: [], unblocked: [] }, stash_time: 5e6 };
const RECORDS_WITH_STASHES_AND_MLBF = [
  MLBF_RECORD,
  MLBF_SOFTBLOCK_RECORD,
  OLDEST_STASH,
  NEWEST_STASH,
];

const ExtensionBlocklistMLBF = getExtensionBlocklistMLBF();

function assertTelemetryScalars(expectedScalars) {
  // On Android, we only report to the Glean system telemetry system.
  if (IS_ANDROID_BUILD) {
    info(
      `Skip assertions on collected samples for ${expectedScalars} on android builds`
    );
    return;
  }
  let scalars = TelemetryTestUtils.getProcessScalars("parent");
  for (const scalarName of Object.keys(expectedScalars || {})) {
    equal(
      scalars[scalarName],
      expectedScalars[scalarName],
      `Got the expected value for ${scalarName} scalar`
    );
  }
}

add_setup(async function setup() {
  if (!IS_ANDROID_BUILD) {
    // FOG needs a profile directory to put its data in.
    do_get_profile();
    // FOG needs to be initialized in order for data to flow.
    Services.fog.initializeFOG();
  }
  await TelemetryController.testSetup();
  await promiseStartupManager();

  // Disable the packaged record and attachment to make sure that the test
  // will not fall back to the packaged attachments.
  Downloader._RESOURCE_BASE_URL = "invalid://bogus";
});

add_task(async function test_initialization() {
  Services.fog.testResetFOG();
  ExtensionBlocklistMLBF.ensureInitialized();

  Assert.equal(
    undefined,
    Glean.blocklist.lastModifiedRsAddonsMblf.testGetValue()
  );
  Assert.equal(undefined, Glean.blocklist.mlbfSource.testGetValue());
  Assert.equal(undefined, Glean.blocklist.mlbfSoftblocksSource.testGetValue());
  Assert.equal(undefined, Glean.blocklist.mlbfGenerationTime.testGetValue());
  Assert.equal(undefined, Glean.blocklist.mlbfStashTimeOldest.testGetValue());
  Assert.equal(undefined, Glean.blocklist.mlbfStashTimeNewest.testGetValue());

  assertTelemetryScalars({
    "blocklist.mlbf_source": undefined,
    "blocklist.mlbf_softblocks_source": undefined,
  });
});

// Test what happens if there is no blocklist data at all.
add_task(async function test_without_mlbf() {
  Services.fog.testResetFOG();
  // Add one (invalid) value to the blocklist, to prevent the RemoteSettings
  // client from importing the JSON dump (which could potentially cause the
  // test to fail due to the unexpected imported records).
  await AddonTestUtils.loadBlocklistRawData({ extensionsMLBF: [{}] });
  Assert.equal("unknown", Glean.blocklist.mlbfSource.testGetValue());
  Assert.equal("unknown", Glean.blocklist.mlbfSoftblocksSource.testGetValue());

  Assert.equal(0, Glean.blocklist.mlbfGenerationTime.testGetValue().getTime());
  Assert.equal(0, Glean.blocklist.mlbfStashTimeOldest.testGetValue().getTime());
  Assert.equal(0, Glean.blocklist.mlbfStashTimeNewest.testGetValue().getTime());

  assertTelemetryScalars({
    "blocklist.mlbf_source": "unknown",
    "blocklist.mlbf_softblocks_source": "unknown",
  });
});

// Test the telemetry that would be recorded in the common case.
add_task(async function test_common_good_case_with_stashes() {
  Services.fog.testResetFOG();
  // The exact content of the attachment does not matter in this test, as long
  // as the data is valid.
  await ExtensionBlocklistMLBF._client.db.saveAttachment(
    ExtensionBlocklistMLBF.RS_ATTACHMENT_ID,
    { record: MLBF_RECORD, blob: await load_mlbf_record_as_blob() }
  );
  await ExtensionBlocklistMLBF._client.db.saveAttachment(
    ExtensionBlocklistMLBF.RS_SOFTBLOCKS_ATTACHMENT_ID,
    {
      record: MLBF_SOFTBLOCK_RECORD,
      blob: await load_mlbf_record_as_blob("mlbf-softblocked1.bin"),
    }
  );
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: RECORDS_WITH_STASHES_AND_MLBF,
  });
  Assert.equal("cache_match", Glean.blocklist.mlbfSource.testGetValue());
  Assert.equal(
    "cache_match",
    Glean.blocklist.mlbfSoftblocksSource.testGetValue()
  );
  Assert.equal(
    MLBF_RECORD.generation_time,
    Glean.blocklist.mlbfGenerationTime.testGetValue().getTime()
  );
  Assert.equal(
    OLDEST_STASH.stash_time,
    Glean.blocklist.mlbfStashTimeOldest.testGetValue().getTime()
  );
  Assert.equal(
    NEWEST_STASH.stash_time,
    Glean.blocklist.mlbfStashTimeNewest.testGetValue().getTime()
  );
  assertTelemetryScalars({
    "blocklist.mlbf_source": "cache_match",
    "blocklist.mlbf_softblocks_source": "cache_match",
  });

  // The records and cached attachment carries over to the next tests.
});

// Test what happens when there are no stashes in the collection itself.
add_task(async function test_without_stashes() {
  Services.fog.testResetFOG();
  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [MLBF_RECORD, MLBF_SOFTBLOCK_RECORD],
  });

  Assert.equal("cache_match", Glean.blocklist.mlbfSource.testGetValue());
  Assert.equal(
    "cache_match",
    Glean.blocklist.mlbfSoftblocksSource.testGetValue()
  );
  Assert.equal(
    MLBF_RECORD.generation_time,
    Glean.blocklist.mlbfGenerationTime.testGetValue().getTime()
  );
  Assert.equal(
    MLBF_SOFTBLOCK_RECORD.generation_time,
    Glean.blocklist.mlbfSoftblocksGenerationTime.testGetValue().getTime()
  );

  Assert.equal(0, Glean.blocklist.mlbfStashTimeOldest.testGetValue().getTime());
  Assert.equal(0, Glean.blocklist.mlbfStashTimeNewest.testGetValue().getTime());

  assertTelemetryScalars({
    "blocklist.mlbf_source": "cache_match",
    "blocklist.mlbf_softblocks_source": "cache_match",
  });
});

// Test what happens when the collection was inadvertently emptied,
// but still with a cached mlbf from before.
add_task(async function test_without_collection_but_cache() {
  Services.fog.testResetFOG();
  await AddonTestUtils.loadBlocklistRawData({
    // Insert a dummy record with a value of last_modified which is higher than
    // any value of last_modified in addons-bloomfilters.json, to prevent the
    // blocklist implementation from automatically falling back to the packaged
    // JSON dump.
    extensionsMLBF: [{ last_modified: Date.now() }],
  });
  Assert.equal("cache_fallback", Glean.blocklist.mlbfSource.testGetValue());
  Assert.equal(
    "cache_fallback",
    Glean.blocklist.mlbfSoftblocksSource.testGetValue()
  );
  Assert.equal(
    MLBF_RECORD.generation_time,
    Glean.blocklist.mlbfGenerationTime.testGetValue().getTime()
  );
  Assert.equal(
    MLBF_SOFTBLOCK_RECORD.generation_time,
    Glean.blocklist.mlbfSoftblocksGenerationTime.testGetValue().getTime()
  );

  Assert.equal(0, Glean.blocklist.mlbfStashTimeOldest.testGetValue().getTime());
  Assert.equal(0, Glean.blocklist.mlbfStashTimeNewest.testGetValue().getTime());

  assertTelemetryScalars({
    "blocklist.mlbf_source": "cache_fallback",
    "blocklist.mlbf_softblocks_source": "cache_fallback",
  });
});
