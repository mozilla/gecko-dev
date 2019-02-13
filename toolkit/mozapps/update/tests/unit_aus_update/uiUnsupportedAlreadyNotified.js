/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://testing-common/MockRegistrar.jsm");

function run_test() {
  setupTestCommon();

  debugDump("testing nsIUpdatePrompt notifications should not be displayed " +
            "when showUpdateAvailable is called for an unsupported system " +
            "update when the unsupported notification has already been " +
            "shown (bug 843497)");

  setUpdateURLOverride();
  // The mock XMLHttpRequest is MUCH faster
  overrideXHR(callHandleEvent);
  standardInit();

  let windowWatcherCID =
    MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1",
                           WindowWatcher);
  let windowMediatorCID =
    MockRegistrar.register("@mozilla.org/appshell/window-mediator;1",
                           WindowMediator);
  do_register_cleanup(() => {
    MockRegistrar.unregister(windowWatcherCID);
    MockRegistrar.unregister(windowMediatorCID);
  });

  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, false);
  Services.prefs.setBoolPref(PREF_APP_UPDATE_NOTIFIEDUNSUPPORTED, true);
  // This preference is used to determine when the background update check has
  // completed since a successful check will clear the preference.
  Services.prefs.setIntPref(PREF_APP_UPDATE_BACKGROUNDERRORS, 1);

  gResponseBody = getRemoteUpdatesXMLString("  <update type=\"major\" " +
                                            "name=\"Unsupported Update\" " +
                                            "unsupported=\"true\" " +
                                            "detailsURL=\"" + URL_HOST +
                                            "\"></update>\n");
  gAUS.notify(null);
  do_execute_soon(check_test);
}

function check_test() {
  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_BACKGROUNDERRORS)) {
    do_execute_soon(check_test);
    return;
  }
  Assert.ok(true,
            PREF_APP_UPDATE_BACKGROUNDERRORS + " preference should not exist");

  doTestFinish();
}

// Callback function used by the custom XMLHttpRequest implementation to
// call the nsIDOMEventListener's handleEvent method for onload.
function callHandleEvent(aXHR) {
  aXHR.status = 400;
  aXHR.responseText = gResponseBody;
  try {
    let parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(Ci.nsIDOMParser);
    aXHR.responseXML = parser.parseFromString(gResponseBody, "application/xml");
  } catch (e) {
  }
  let e = { target: aXHR };
  aXHR.onload(e);
}

function check_showUpdateAvailable() {
  do_throw("showUpdateAvailable should not have called openWindow!");
}

const WindowWatcher = {
  openWindow: function(aParent, aUrl, aName, aFeatures, aArgs) {
    check_showUpdateAvailable();
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowWatcher])
};

const WindowMediator = {
  getMostRecentWindow: function(aWindowType) {
    return null;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowMediator])
};
