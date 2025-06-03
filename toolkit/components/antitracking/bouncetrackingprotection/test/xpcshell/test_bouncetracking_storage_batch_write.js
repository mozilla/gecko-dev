/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// const { Services } = ChromeUtils.importESModule("resource://gre/modules/Services.sys.mjs");
const { Sqlite } = ChromeUtils.importESModule(
  "resource://gre/modules/Sqlite.sys.mjs"
);

// The path to the sqlite database file.
let databasePath;

/**
 * Count the number of entries in the database.
 * @returns {number} The number of entries in the database.
 */
async function countDatabaseEntries() {
  let db = await Sqlite.openConnection({ path: databasePath });
  let result = await db.execute("SELECT COUNT(*) as count FROM sites");
  await db.close();
  return result[0].getInt64("count");
}

// The maximum number of pending updates before a database flush is triggered.
const MAX_PENDING_UPDATES = Services.prefs.getIntPref(
  "privacy.bounceTrackingProtection.storage.maxPendingUpdates"
);

/**
 * Wait for an observer message.
 * @param {string} topic The topic to wait for.
 * @returns {Promise<{subject: any, topic: string, data: string}>} A promise that resolves to the subject, topic, and data of the message.
 */
function waitForObserverMessage(topic) {
  return new Promise(resolve => {
    let observer = {
      observe(subject, observedTopic, data) {
        if (observedTopic === topic) {
          Services.obs.removeObserver(observer, topic);
          resolve({ subject, topic, data });
        }
      },
    };
    Services.obs.addObserver(observer, topic);
  });
}

/**
 * Wait for the database flush to complete.
 */
async function waitForDBFlush() {
  await waitForObserverMessage("bounce-tracking-protection-storage-flushed");
}

/**
 * Wait for the database flush to be skipped because the buffer isn't full yet.
 * @returns {number} The number of pending updates when the flush was skipped.
 */
async function waitForDBSkipFlush() {
  let { data } = await waitForObserverMessage(
    "bounce-tracking-protection-storage-flush-skipped"
  );
  return Number.parseInt(data, 10);
}

add_setup(async function () {
  // BTP storage needs a profile directory to work. That's where the sqlite
  // database file is stored.
  do_get_profile();

  // Get the sqlite database file path.
  let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let dbFile = profileDir.clone();
  dbFile.append("bounce-tracking-protection.sqlite");
  databasePath = dbFile.path;

  // Enable BTP
  Services.prefs.setIntPref(
    "privacy.bounceTrackingProtection.mode",
    Ci.nsIBounceTrackingProtection.MODE_ENABLED
  );

  // Enable test mode for test-only observer notifications.
  Services.prefs.setBoolPref(
    "privacy.bounceTrackingProtection.enableTestMode",
    true
  );
});

/**
 * Test that the storage flushes when the number of pending updates reaches
 * maxPendingUpdates, with both user activation and bounce tracker entries.
 */
add_task(async function test_batch_write_basic() {
  // Reset all state
  let btp = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
    Ci.nsIBounceTrackingProtection
  );
  btp.clearAll();

  let count = await countDatabaseEntries();
  Assert.equal(count, 0, "Database should be empty after clearAll");

  // Add entries up to MAX_PENDING_UPDATES - 1
  let halfBatch = Math.floor((MAX_PENDING_UPDATES - 1) / 2);
  for (let i = 0; i < halfBatch; i++) {
    // Add user activation
    let skipFlushPromise = waitForDBSkipFlush();
    btp.testAddUserActivation({}, `example-foo${i}.com`, Date.now() * 1000);
    await skipFlushPromise;

    // Add bounce tracker candidate
    skipFlushPromise = waitForDBSkipFlush();
    btp.testAddBounceTrackerCandidate(
      {},
      `example-bar${i}.com`,
      Date.now() * 1000
    );

    // Now there should be two additional pending updates.
    let pendingUpdatesCount = await skipFlushPromise;
    let expectedPendingUpdatesCount = (i + 1) * 2;
    Assert.equal(
      pendingUpdatesCount,
      expectedPendingUpdatesCount,
      `Should have ${expectedPendingUpdatesCount} pending updates.`
    );
  }
  info(
    `Added ${halfBatch * 2} entries (${halfBatch} user activations and ${halfBatch} bounce trackers)`
  );

  count = await countDatabaseEntries();
  Assert.equal(
    count,
    0,
    "Database should still be empty before reaching maxPendingUpdates"
  );

  let dbWritePromise = waitForDBFlush();
  info("Adding final entry to trigger flush");
  btp.testAddUserActivation({}, "example-foo-final.com", Date.now() * 1000);
  await dbWritePromise;
  info("Database write completed");

  count = await countDatabaseEntries();
  Assert.equal(
    count,
    MAX_PENDING_UPDATES,
    "Database should have all entries after flush"
  );
  info(`Verified database has all ${MAX_PENDING_UPDATES} entries after flush`);

  // Cleanup
  btp.clearAll();
});

/**
 * Same as previous test, but with multiple flushes.
 */
add_task(async function test_batch_write_multiple_flushes() {
  // Reset all state
  let btp = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
    Ci.nsIBounceTrackingProtection
  );
  btp.clearAll();

  info(
    `Testing multiple flushes with maxPendingUpdates=${MAX_PENDING_UPDATES}`
  );

  // Add entries in multiple batches
  for (let batch = 0; batch < 3; batch++) {
    info(`Starting batch ${batch + 1} of 3`);
    // Add entries up to MAX_PENDING_UPDATES - 1
    let halfBatch = Math.floor((MAX_PENDING_UPDATES - 1) / 2);
    for (let i = 0; i < halfBatch; i++) {
      let skipFlushPromise = waitForDBSkipFlush();
      btp.testAddUserActivation(
        {},
        `example-foo${batch}_${i}.com`,
        Date.now() * 1000
      );
      await skipFlushPromise;

      skipFlushPromise = waitForDBSkipFlush();
      btp.testAddBounceTrackerCandidate(
        {},
        `example-bar${batch}_${i}.com`,
        Date.now() * 1000
      );
      await skipFlushPromise;
    }
    info(`Added ${halfBatch * 2} entries for batch ${batch + 1}`);

    let count = await countDatabaseEntries();
    Assert.equal(
      count,
      batch * MAX_PENDING_UPDATES,
      `Database should have ${batch * MAX_PENDING_UPDATES} entries before next flush`
    );

    let dbWritePromise = waitForDBFlush();

    // Add one more to trigger flush and wait for write to complete
    info(`Adding final entry for batch ${batch + 1} to trigger flush`);
    btp.testAddUserActivation(
      {},
      `example${batch}_final.com`,
      Date.now() * 1000
    );
    await dbWritePromise;
    info(`Batch ${batch + 1} write completed`);

    count = await countDatabaseEntries();
    Assert.equal(
      count,
      (batch + 1) * MAX_PENDING_UPDATES,
      `Database should have ${(batch + 1) * MAX_PENDING_UPDATES} entries after flush`
    );
  }

  // Cleanup
  btp.clearAll();
});

/**
 * Test that clearing data works correctly with pending updates.
 */
add_task(async function test_batch_write_clear_behavior() {
  // Reset all state
  let btp = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
    Ci.nsIBounceTrackingProtection
  );
  btp.clearAll();

  info(`Testing clear behavior with maxPendingUpdates=${MAX_PENDING_UPDATES}`);

  // Add some entries but don't reach MAX_PENDING_UPDATES
  let halfBatch = Math.floor((MAX_PENDING_UPDATES - 1) / 2);
  for (let i = 0; i < halfBatch; i++) {
    btp.testAddUserActivation({}, `example-foo${i}.com`, Date.now() * 1000);
    btp.testAddBounceTrackerCandidate(
      {},
      `example-bar${i}.com`,
      Date.now() * 1000
    );
  }
  info(
    `Added ${halfBatch * 2} entries (${halfBatch} user activations and ${halfBatch} bounce trackers)`
  );

  // Clear all state.
  info("Clearing all state");
  btp.clearAll();

  info("Adding entry after clearAll to check if pending updates were cleared");
  let flushSkipPromise = waitForDBSkipFlush();
  btp.testAddUserActivation({}, "example-after-clear.com", Date.now() * 1000);
  let pendingUpdatesCount = await flushSkipPromise;
  // There should only be 1 pending update because the other pending updates were cleared through the clearAll.
  Assert.equal(pendingUpdatesCount, 1, "Should only have 1 pending update.");

  // Verify database is empty (pending updates should be cleared)
  let count = await countDatabaseEntries();
  Assert.equal(
    count,
    0,
    "Database should still be empty after clearAll with pending updates"
  );

  // Cleanup
  btp.clearAll();
});
