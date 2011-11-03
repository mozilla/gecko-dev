/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */


function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Write out a minimal database.
  let dbfile = gProfD.clone();
  dbfile.append("addons.sqlite");
  let db = AM_Cc["@mozilla.org/storage/service;1"].
           getService(AM_Ci.mozIStorageService).
           openDatabase(dbfile);

  db.createTable("futuristicSchema",
                 "id INTEGER, " +
                 "sharks TEXT, " +
                 "lasers TEXT");

  db.schemaVersion = 1000;
  db.close();

  Services.obs.addObserver({
    observe: function () {
      Services.obs.removeObserver(this, "addon-repository-shutdown");
      // Check the DB schema has changed once AddonRepository has freed it.
      db = AM_Cc["@mozilla.org/storage/service;1"].
           getService(AM_Ci.mozIStorageService).
           openDatabase(dbfile);
      do_check_eq(db.schemaVersion, 1);
      db.close();
      do_test_finished();
    }
  }, "addon-repository-shutdown", null);

  // Force a connection to the addon database to be opened.
  Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", true);
  AddonRepository.getCachedAddonByID("test1@tests.mozilla.org", function (aAddon) {
    AddonRepository.shutdown();
  });
}
