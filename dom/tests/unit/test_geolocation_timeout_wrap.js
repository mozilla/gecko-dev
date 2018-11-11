var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://testing-common/httpd.js");

var httpserver = null;

function run_test() {
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  prefs.setBoolPref("geo.wifi.scan", false);

  httpserver = new HttpServer();
  httpserver.start(-1);
  prefs.setCharPref("geo.wifi.uri", "http://localhost:" +
                    httpserver.identity.primaryPort + "/geo");
  prefs.setBoolPref("dom.testing.ignore_ipc_principal", true);
  run_test_in_child("./test_geolocation_timeout.js");
}
