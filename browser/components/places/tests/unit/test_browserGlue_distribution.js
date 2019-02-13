/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that nsBrowserGlue correctly imports bookmarks from distribution.ini.
 */

const PREF_SMART_BOOKMARKS_VERSION = "browser.places.smartBookmarksVersion";
const PREF_BMPROCESSED = "distribution.516444.bookmarksProcessed";
const PREF_DISTRIBUTION_ID = "distribution.id";

const TOPICDATA_DISTRIBUTION_CUSTOMIZATION = "force-distribution-customization";
const TOPIC_CUSTOMIZATION_COMPLETE = "distribution-customization-complete";
const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";

function run_test() {
  // Set special pref to load distribution.ini from the profile folder.
  Services.prefs.setBoolPref("distribution.testing.loadFromProfile", true);

  // Copy distribution.ini file to the profile dir.
  let distroDir = gProfD.clone();
  distroDir.leafName = "distribution";
  let iniFile = distroDir.clone();
  iniFile.append("distribution.ini");
  if (iniFile.exists()) {
    iniFile.remove(false);
    print("distribution.ini already exists, did some test forget to cleanup?");
  }

  let testDistributionFile = gTestDir.clone();
  testDistributionFile.append("distribution.ini");
  testDistributionFile.copyTo(distroDir, "distribution.ini");
  Assert.ok(testDistributionFile.exists());

  run_next_test();
}

do_register_cleanup(function () {
  // Remove the distribution file, even if the test failed, otherwise all
  // next tests will import it.
  let iniFile = gProfD.clone();
  iniFile.leafName = "distribution";
  iniFile.append("distribution.ini");
  if (iniFile.exists()) {
    iniFile.remove(false);
  }
  Assert.ok(!iniFile.exists());
});

add_task(function* () {
  // Disable Smart Bookmarks creation.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, -1);

  // Initialize Places through the History Service and check that a new
  // database has been created.
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_CREATE);

  // Force distribution.
  let glue = Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIObserver)
  glue.observe(null, TOPIC_BROWSERGLUE_TEST, TOPICDATA_DISTRIBUTION_CUSTOMIZATION);

  // Test will continue on customization complete notification.
  yield promiseTopicObserved(TOPIC_CUSTOMIZATION_COMPLETE);

  // Check the custom bookmarks exist on menu.
  let menuItem = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0
  });
  Assert.equal(menuItem.title, "Menu Link Before");

  menuItem = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 1 + DEFAULT_BOOKMARKS_ON_MENU
  });
  Assert.equal(menuItem.title, "Menu Link After");

  // Check the custom bookmarks exist on toolbar.
  let toolbarItem = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  Assert.equal(toolbarItem.title, "Toolbar Link Before");

  toolbarItem = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 1 + DEFAULT_BOOKMARKS_ON_TOOLBAR
  });
  Assert.equal(toolbarItem.title, "Toolbar Link After");

  // Check the bmprocessed pref has been created.
  Assert.ok(Services.prefs.getBoolPref(PREF_BMPROCESSED));

  // Check distribution prefs have been created.
  Assert.equal(Services.prefs.getCharPref(PREF_DISTRIBUTION_ID), "516444");
});
