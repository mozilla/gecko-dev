/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;
const CC = Components.Constructor;

const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let gAppInfo;
function createAppInfo(id, name, version, platformVersion) {
  gAppInfo = {
    // nsIXULAppInfo
    vendor: "Mozilla",
    name: name,
    ID: id,
    version: version,
    appBuildID: "2007010101",
    platformVersion: platformVersion ? platformVersion : "1.0",
    platformBuildID: "2007010101",

    // nsIXULRuntime
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",
    invalidateCachesOnRestart: function invalidateCachesOnRestart() {
      // Do nothing
    },

    // nsICrashReporter
    annotations: {},

    annotateCrashReport: function(key, data) {
      this.annotations[key] = data;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo,
                                           Ci.nsIXULRuntime,
                                           Ci.nsICrashReporter,
                                           Ci.nsISupports])
  };

  var XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null)
        throw Cr.NS_ERROR_NO_AGGREGATION;
      return gAppInfo.QueryInterface(iid);
    }
  };
  var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                            XULAPPINFO_CONTRACTID, XULAppInfoFactory);
}

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

let gClient, gActor;

function connect(onDone) {
  // Initialize a loopback remote protocol connection
  DebuggerServer.init(function () { return true; });
  // We need to register browser actors to have `listTabs` working
  // and also have a root actor
  DebuggerServer.addBrowserActors();

  // Setup client and actor used in all tests
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function onConnect() {
    gClient.listTabs(function onListTabs(aResponse) {
      gActor = aResponse.webappsActor;
      onDone();
    });
  });
}

function webappActorRequest(request, onResponse) {
  if (!gActor) {
    connect(webappActorRequest.bind(null, request, onResponse));
    return;
  }

  request.to = gActor;
  gClient.request(request, onResponse);
}

// Install a test packaged webapp from data folder
function installTestApp(zipName, appId, onDone) {
  // Copy our package to tmp folder, where the actor retrieves it
  let zip = do_get_file("data/" + zipName);
  let appDir = FileUtils.getDir("TmpD", ["b2g", appId], true, true);
  zip.copyTo(appDir, "application.zip");

  let request = {type: "install", appId: appId};
  webappActorRequest(request, function (aResponse) {
    do_check_eq(aResponse.appId, appId);
    if ("error" in aResponse) {
      do_throw("Error: " + aResponse.error);
    }
    if ("message" in aResponse) {
      do_throw("Error message: " + aResponse.message);
    }
    do_check_false("error" in aResponse);

    onDone();
  });
};

function setup() {
  // We have to setup a profile, otherwise indexed db used by webapps
  // will throw random exception when trying to get profile folder
  do_get_profile();

  // The webapps dir isn't registered on b2g xpcshell tests,
  // we have to manually set it to the directory service.
  do_get_webappsdir();

  // We also need a valid nsIXulAppInfo service as Webapps.jsm is querying it
  Components.utils.import("resource://testing-common/AppInfo.jsm");
  updateAppInfo();

  // We have to toggle this flag in order to have apps being listed in getAll
  // as only launchable apps are returned
  Components.utils.import('resource://gre/modules/Webapps.jsm');
  DOMApplicationRegistry.allAppsLaunchable = true;

  // Mock WebappOSUtils
  Cu.import("resource://gre/modules/WebappOSUtils.jsm");
  WebappOSUtils.getPackagePath = function(aApp) {
    return aApp.basePath + "/" + aApp.id;
  }
}

function do_get_webappsdir() {
  var webappsDir = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  webappsDir.append("test_webapps");
  if (!webappsDir.exists())
    webappsDir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("755", 8));

  var coreAppsDir = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  coreAppsDir.append("test_coreapps");
  if (!coreAppsDir.exists())
    coreAppsDir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("755", 8));

  // Register our own provider for the profile directory.
  // It will return our special docshell profile directory.
  var provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == "webappsDir") {
        return webappsDir.clone();
      }
      else if (prop == "coreAppsDir") {
        return coreAppsDir.clone();
      }
      throw Cr.NS_ERROR_FAILURE;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
          iid.equals(Ci.nsISupports)) {
        return this;
      }
      throw Cr.NS_ERROR_NO_INTERFACE;
    }
  };
  Services.dirsvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);
}


