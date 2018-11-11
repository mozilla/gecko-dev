var Cc = Components.classes;
var Ci = Components.interfaces;

const providerCID = Components.ID("{14aa4b81-e266-45cb-88f8-89595dece114}");
const providerContract = "@mozilla.org/geolocation/provider;1";

const categoryName = "geolocation-provider";

var provider = {
  QueryInterface: function eventsink_qi(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIFactory) ||
        iid.equals(Components.interfaces.nsIGeolocationProvider))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  createInstance: function eventsink_ci(outer, iid) {
    if (outer)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },
  lockFactory: function eventsink_lockf(lock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  startup: function() {
  },
  watch: function() {
  },
  shutdown: function() {
  },
  setHighAccuracy: function(enable) {
    this._isHigh = enable;
    if (enable) {
      this._seenHigh = true;
      do_execute_soon(stop_high_accuracy_watch);
    }
  },
  _isHigh: false,
  _seenHigh: false
};

var runningInParent = true;
try {
  runningInParent = Components.classes["@mozilla.org/xre/runtime;1"].
                    getService(Components.interfaces.nsIXULRuntime).processType
                    == Components.interfaces.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}
catch (e) { }

function successCallback()
{
  do_check_true(false);
  do_test_finished();
}

function errorCallback()
{
  do_check_true(false);
  do_test_finished();
}

var geolocation;
var watchID2;

function run_test()
{
  if (runningInParent) {
    // XPCShell does not get a profile by default. The geolocation service
    // depends on the settings service which uses IndexedDB and IndexedDB
    // needs a place where it can store databases.
    do_get_profile();

    Components.manager.nsIComponentRegistrar.registerFactory(providerCID,
      "Unit test geo provider", providerContract, provider);
    var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);
    catMan.nsICategoryManager.addCategoryEntry(categoryName, "unit test",
                                               providerContract, false, true);

    var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
    prefs.setBoolPref("dom.testing.ignore_ipc_principal", true);
    prefs.setBoolPref("geo.wifi.scan", false);
  }

  do_test_pending();

  geolocation = Cc["@mozilla.org/geolocation;1"].createInstance(Ci.nsISupports);
  let watchID1 = geolocation.watchPosition(successCallback, errorCallback);
  watchID2 = geolocation.watchPosition(successCallback, errorCallback,
                                       {enableHighAccuracy: true});

  if (!runningInParent) {
    do_await_remote_message('high_acc_enabled', stop_high_accuracy_watch);
  }
}

function stop_high_accuracy_watch() {
    geolocation.clearWatch(watchID2);
    check_results();
    do_test_finished();
}

function check_results()
{
  if (runningInParent) {
    // check the provider was set to high accuracy during the test
    do_check_true(provider._seenHigh);
    // check the provider is not currently set to high accuracy
    do_check_false(provider._isHigh);
  }
  do_test_finished();
}
