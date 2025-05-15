function run_test() {
  // XPCShell does not get a profile by default. The ContentParent
  // depends on the remote settings service which uses IndexedDB and
  // IndexedDB needs a place where it can store databases.
  do_get_profile();

  Services.prefs.setBoolPref("geo.provider.network.scan", false);
  Services.prefs.setCharPref("geo.provider.network.url", "UrlNotUsedHere");

  run_test_in_child("./test_geolocation_position_unavailable.js");
}
