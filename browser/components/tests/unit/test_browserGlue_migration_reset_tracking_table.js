/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const LEVEL_1_TRACKING_TABLE =
  "moztest-track-simple,ads-track-digest256,social-track-digest256,analytics-track-digest256";
const LEVEL_2_TRACKING_TABLE =
  LEVEL_1_TRACKING_TABLE + ",content-track-digest256";
const CUSTOM_BLOCK_LIST_PREF =
  "browser.contentblocking.customBlockList.preferences.ui.enabled";
const TRACKING_TABLE_PREF = "urlclassifier.trackingTable";
const BROWSER_VERSION = 155;

const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);

add_setup(() => {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.migration.version");
    Services.prefs.clearUserPref(CUSTOM_BLOCK_LIST_PREF);
    Services.prefs.clearUserPref(TRACKING_TABLE_PREF);
  });
});

add_task(async function test_migration_reset_tracking_table() {
  // Set up user having level 2 tracking table
  Services.prefs.setBoolPref(CUSTOM_BLOCK_LIST_PREF, true);
  Services.prefs.setStringPref(TRACKING_TABLE_PREF, LEVEL_2_TRACKING_TABLE);
  Services.prefs.setIntPref("browser.migration.version", BROWSER_VERSION);

  // Simulate the migration process
  gBrowserGlue.observe(null, "browser-glue-test", "force-ui-migration");

  // Check that the migration has reset the tracking table to level 1 and customBlockList is set to false
  Assert.ok(
    !Services.prefs.getBoolPref(CUSTOM_BLOCK_LIST_PREF, false),
    "browser.contentblocking.customBlockList.preferences.ui.enabled pref should be false"
  );
  Assert.ok(
    (Services.prefs.getStringPref(TRACKING_TABLE_PREF),
    LEVEL_1_TRACKING_TABLE) === LEVEL_1_TRACKING_TABLE,
    "urlclassifier.trackingTable pref should be reset to level 1"
  );
});
