/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let btp;

async function assertProtectionsPanelCounter(value) {
  // Open a tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  await openProtectionsPanel();

  let trackerCounterBox = document.getElementById(
    "protections-popup-trackers-blocked-counter-box"
  );
  let trackerCounterDesc = document.getElementById(
    "protections-popup-trackers-blocked-counter-description"
  );

  if (value == 0) {
    ok(
      BrowserTestUtils.isHidden(trackerCounterBox),
      "The blocked tracker counter is hidden if there is no blocked tracker."
    );
  } else {
    ok(
      BrowserTestUtils.isVisible(trackerCounterBox),
      "The blocked tracker counter is shown if there are blocked trackers."
    );
    is(
      trackerCounterDesc.textContent,
      `${value} Blocked`,
      "The blocked tracker counter is correct."
    );
  }

  await closeProtectionsPanel();
  BrowserTestUtils.removeTab(tab);
}

add_setup(async function () {
  btp = Cc["@mozilla.org/bounce-tracking-protection;1"].getService(
    Ci.nsIBounceTrackingProtection
  );
  // Reset global bounce tracking state.
  btp.clearAll();

  // Clear the tracking database.
  await TrackingDBService.clearAll();
});

add_task(
  async function test_purged_bounce_trackers_increment_tracker_counter() {
    info("Before purging test that there are no blocked trackers.");
    await assertProtectionsPanelCounter(0);

    info("Add bounce trackers to be purged.");
    let now = Date.now();
    let bounceTrackingGracePeriodSec = Services.prefs.getIntPref(
      "privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec"
    );
    let timestampOutsideGracePeriodThreeDays =
      now - (bounceTrackingGracePeriodSec + 60 * 60 * 24 * 3) * 1000;

    const BOUNCE_TRACKERS = ["example.org", "example.com", "example.net"];

    BOUNCE_TRACKERS.forEach(siteHost => {
      btp.testAddBounceTrackerCandidate(
        {},
        siteHost,
        timestampOutsideGracePeriodThreeDays * 1000
      );
    });

    info("Run PurgeBounceTrackers");
    await btp.testRunPurgeBounceTrackers();

    info("After purging test that the counter is showing and incremented.");
    await assertProtectionsPanelCounter(BOUNCE_TRACKERS.length);

    registerCleanupFunction(async () => {
      // Reset global bounce tracking state.
      btp.clearAll();
      // Clear anti tracking db
      await TrackingDBService.clearAll();
    });
  }
);
