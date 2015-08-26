// ----------------------------------------------------------------------------
// Tests that navigating to a new origin cancels ongoing installs.

// Block the modal install UI from showing.
let InstallPrompt = {
  confirm: function(aBrowser, aUri, aInstalls, aCount) {
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstallPrompt]),

  classID: Components.ID("{405f3c55-241f-40df-97f1-a6e60e250ec5}"),

  factory: {
    registrar: Components.manager.QueryInterface(Ci.nsIComponentRegistrar),

    register: function() {
      this.registrar.registerFactory(InstallPrompt.classID, "InstallPrompt",
                                     "@mozilla.org/addons/web-install-prompt;1",
                                     this);
    },

    unregister: function() {
      this.registrar.unregisterFactory(InstallPrompt.classID, this);
    },

    // nsIFactory
    createInstance: function(aOuter, aIID) {
      if (aOuter) {
        throw Components.Exception("Class does not allow aggregation",
                                   Components.results.NS_ERROR_NO_AGGREGATION);
      }
      return InstallPrompt.QueryInterface(aIID);
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory])
  }
};

function test() {
  InstallPrompt.factory.register();
  registerCleanupFunction(() => {
    InstallPrompt.factory.unregister();
  });

  Harness.downloadProgressCallback = download_progress;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": TESTROOT + "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
}

function download_progress(addon, value, maxValue) {
  gBrowser.loadURI(TESTROOT2 + "enabled.html");
}

function install_ended(install, addon) {
  ok(false, "Should not have seen installs complete");
}

function finish_test(count) {
  is(count, 0, "No add-ons should have been successfully installed");

  Services.perms.remove("http://example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
}
