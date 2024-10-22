function run_test() {
  Services.prefs.setBoolPref("network.url.useDefaultURI", true);
  Services.prefs.setBoolPref(
    "network.url.simple_uri_unknown_schemes_enabled",
    true
  );
  Services.prefs.setCharPref(
    "network.url.simple_uri_unknown_schemes",
    "simpleprotocol,otherproto"
  );

  run_test_in_child("../unit/test_simple_unknown_uris.js");
}
