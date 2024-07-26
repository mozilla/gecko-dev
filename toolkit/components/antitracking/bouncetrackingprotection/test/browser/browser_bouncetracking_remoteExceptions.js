/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

// Name of the RemoteSettings collection containing exceptions.
const COLLECTION_NAME = "bounce-tracking-protection-exceptions";

// RemoteSettings collection db.
let db;

/**
 * Compare two string arrays ignoring order.
 * @param {string[]} arr1
 * @param {string[]} arr2
 * @returns {boolean} - Whether the arrays match.
 */
const strArrayMatches = (arr1, arr2) =>
  arr1.length === arr2.length &&
  arr1.sort().every((value, index) => value === arr2.sort()[index]);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });

  // Start with an empty RS collection.
  info(`Initializing RemoteSettings collection "${COLLECTION_NAME}".`);
  db = RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, Date.now(), [], { clear: true });
});

/**
 * Run a bounce test.
 * @param {boolean} expectTrackerPurged - Whether the bounce tracker is expected
 * to be purged.
 */
async function runPurgeTest(expectTrackerPurged) {
  ok(!SiteDataTestUtils.hasCookies(ORIGIN_TRACKER), "No cookies initially.");

  await runTestBounce({
    bounceType: "client",
    setState: "cookie-client",
    postBounceCallback: () => {
      ok(
        SiteDataTestUtils.hasCookies(ORIGIN_TRACKER),
        "Cookie added in bounce."
      );
    },
    skipSiteDataCleanup: true,
    expectPurge: expectTrackerPurged,
  });

  info("After purging the site shouldn't have any data.");

  let hasCookies = SiteDataTestUtils.hasCookies(ORIGIN_TRACKER);
  if (expectTrackerPurged) {
    ok(!hasCookies, "Cookies purged.");
  } else {
    ok(hasCookies, "Cookies not purged.");
  }

  info("Cleanup");
  bounceTrackingProtection.clearAll();
  await SiteDataTestUtils.clear();
}

/**
 * Wait until the BTP allow-list matches the expected state.
 * @param {string[]} allowedSiteHosts - (Unordered) host list to match.
 */
async function waitForAllowListState(allowedSiteHosts) {
  // Ensure the site host exception list has been imported correctly.
  await BrowserTestUtils.waitForCondition(() => {
    return strArrayMatches(
      bounceTrackingProtection.testGetSiteHostExceptions(),
      allowedSiteHosts
    );
  }, "Waiting for exceptions to be imported.");
  Assert.deepEqual(
    bounceTrackingProtection.testGetSiteHostExceptions().sort(),
    allowedSiteHosts.sort(),
    "Imported the correct site host exceptions"
  );
}

/**
 * Dispatch a RemoteSettings "sync" event.
 * @param {Object} data - The event's data payload.
 * @param {Object} [data.created] - Records that were created.
 * @param {Object} [data.updated] - Records that were updated.
 * @param {Object} [data.deleted] - Records that were removed.
 */
async function remoteSettingsSync({ created, updated, deleted }) {
  await RemoteSettings(COLLECTION_NAME).emit("sync", {
    data: {
      created,
      updated,
      deleted,
    },
  });
}

// Integration test that adds remote settings allowlist entries and checks
// whether bounce trackers get purged.
add_task(async function test_remote_exceptions_and_purge() {
  info("Run purge test without any exceptions.");
  await runPurgeTest(true);

  info("Add exceptions via RemoteSettings");
  // At this point the exception list component has already initialized. Changes
  // made here should be picked up via the onSync listener.
  let entryTrackerA = await db.create({ siteHost: SITE_TRACKER });
  let entryTrackerB = await db.create({ siteHost: SITE_TRACKER_B });
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({ created: [entryTrackerA, entryTrackerB] });
  await waitForAllowListState([SITE_TRACKER, SITE_TRACKER_B]);

  info(
    "Run the purge test again, this time no data should be purged because the tracker is on the exception list."
  );
  await runPurgeTest(false);

  info(
    "Remove tracker from exception list and run purge test again. Data should be purged again."
  );
  await db.delete(entryTrackerA.id);
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({ deleted: [entryTrackerA] });

  // Ensure the site host exception list has been imported correctly.
  await waitForAllowListState([SITE_TRACKER_B]);

  await runPurgeTest(true);

  info("Cleanup");
  await db.clear();
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({ deleted: [entryTrackerB] });
  await waitForAllowListState([]);
});

// Unit test for syncing the remote settings list between BTPRemoteExceptionList
// and BounceTrackingProtection.
add_task(async function test_remote_exception_updates() {
  // Run an empty purge to ensure the list has been initialized. The remote
  // exception list is lazily constructed the first time a purge runs. It's
  // guaranteed to be initialized after the purge method resolves.
  await bounceTrackingProtection.testRunPurgeBounceTrackers();

  info("Create foo.com, bar.com");
  let entryTrackerA = await db.create({ siteHost: "foo.com" });
  let entryTrackerB = await db.create({ siteHost: "bar.com" });
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({ created: [entryTrackerA, entryTrackerB] });
  await waitForAllowListState(["foo.com", "bar.com"]);

  info("Update foo.com -> foo2.com");
  let entryTrackerAUpdated = { ...entryTrackerA };
  entryTrackerAUpdated.siteHost = "foo2.com";
  await db.update(entryTrackerA);
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({
    updated: [{ old: entryTrackerA, new: entryTrackerAUpdated }],
  });
  await waitForAllowListState(["foo2.com", "bar.com"]);

  info("Create example.com, remove foo2.com, bar.com");
  let entryTrackerC = await db.create({ siteHost: "example.com" });
  await db.delete(entryTrackerAUpdated.id);
  await db.delete(entryTrackerB.id);
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({
    created: [entryTrackerC],
    deleted: [entryTrackerAUpdated, entryTrackerB],
  });
  await waitForAllowListState(["example.com"]);

  info("Remove example.com, no hosts remain.");
  await db.delete(entryTrackerC.id);
  await db.importChanges({}, Date.now());
  await remoteSettingsSync({ deleted: [entryTrackerC] });
  await waitForAllowListState([]);

  info("Cleanup");
  await db.clear();
  await db.importChanges({}, Date.now());
});
