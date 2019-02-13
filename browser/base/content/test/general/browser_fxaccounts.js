/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let {Log} = Cu.import("resource://gre/modules/Log.jsm", {});
let {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
let {fxAccounts} = Cu.import("resource://gre/modules/FxAccounts.jsm", {});
let FxAccountsCommon = {};
Cu.import("resource://gre/modules/FxAccountsCommon.js", FxAccountsCommon);

const TEST_ROOT = "http://example.com/browser/browser/base/content/test/general/";

// instrument gFxAccounts to send observer notifications when it's done
// what it does.
(function() {
  let unstubs = {}; // The original functions we stub out.

  // The stub functions.
  let stubs = {
    updateAppMenuItem() {
      return unstubs['updateAppMenuItem'].call(gFxAccounts).then(() => {
        Services.obs.notifyObservers(null, "test:browser_fxaccounts:updateAppMenuItem", null);
      });
    },
    // Opening preferences is trickier than it should be as leaks are reported
    // due to the promises it fires off at load time  and there's no clear way to
    // know when they are done.
    // So just ensure openPreferences is called rather than whether it opens.
    openPreferences() {
      Services.obs.notifyObservers(null, "test:browser_fxaccounts:openPreferences", null);
    },
    openAccountsPage(action, params) {
      Services.obs.notifyObservers(null, "test:browser_fxaccounts:openAccountsPage", null);
    }
  };

  for (let name in stubs) {
    unstubs[name] = gFxAccounts[name];
    gFxAccounts[name] = stubs[name];
  }
  // and undo our damage at the end.
  registerCleanupFunction(() => {
    for (let name in unstubs) {
      gFxAccounts[name] = unstubs[name];
    }
    stubs = unstubs = null;
  });
})();

// Other setup/cleanup
let newTab;

Services.prefs.setCharPref("identity.fxaccounts.remote.signup.uri",
                           TEST_ROOT + "accounts_testRemoteCommands.html");
Services.prefs.setBoolPref("identity.fxaccounts.profile_image.enabled", false);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("identity.fxaccounts.remote.signup.uri");
  Services.prefs.clearUserPref("identity.fxaccounts.remote.profile.uri");
  Services.prefs.clearUserPref("identity.fxaccounts.profile_image.enabled");
  gBrowser.removeTab(newTab);
});

add_task(function* initialize() {
  // Set a new tab with something other than about:blank, so it doesn't get reused.
  // We must wait for it to load or the promiseTabOpen() call in the next test
  // gets confused.
  newTab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla", {animate: false});
  yield promiseTabLoaded(newTab);
});

// The elements we care about.
let panelUILabel = document.getElementById("PanelUI-fxa-label");
let panelUIStatus = document.getElementById("PanelUI-fxa-status");
let panelUIFooter = document.getElementById("PanelUI-footer-fxa");

// The tests
add_task(function* test_nouser() {
  let user = yield fxAccounts.getSignedInUser();
  Assert.strictEqual(user, null, "start with no user signed in");
  let promiseUpdateDone = promiseObserver("test:browser_fxaccounts:updateAppMenuItem");
  Services.obs.notifyObservers(null, this.FxAccountsCommon.ONLOGOUT_NOTIFICATION, null);
  yield promiseUpdateDone;

  // Check the world - the FxA footer area is visible as it is offering a signin.
  Assert.ok(isFooterVisible())

  Assert.equal(panelUILabel.getAttribute("label"), panelUIStatus.getAttribute("defaultlabel"));
  Assert.ok(!panelUIStatus.hasAttribute("tooltiptext"), "no tooltip when signed out");
  Assert.ok(!panelUIFooter.hasAttribute("fxastatus"), "no fxsstatus when signed out");
  Assert.ok(!panelUIFooter.hasAttribute("fxaprofileimage"), "no fxaprofileimage when signed out");

  let promiseAccountsOpen = promiseObserver("test:browser_fxaccounts:openAccountsPage");
  panelUIStatus.click();
  yield promiseAccountsOpen;
});

/*
XXX - Bug 1191162 - need a better hawk mock story or this will leak in debug builds.

add_task(function* test_unverifiedUser() {
  let promiseUpdateDone = promiseObserver("test:browser_fxaccounts:updateAppMenuItem");
  yield setSignedInUser(false); // this will fire the observer that does the update.
  yield promiseUpdateDone;

  // Check the world.
  Assert.ok(isFooterVisible())

  Assert.equal(panelUILabel.getAttribute("label"), "foo@example.com");
  Assert.equal(panelUIStatus.getAttribute("tooltiptext"),
               panelUIStatus.getAttribute("signedinTooltiptext"));
  Assert.equal(panelUIFooter.getAttribute("fxastatus"), "signedin");
  let promisePreferencesOpened = promiseObserver("test:browser_fxaccounts:openPreferences");
  panelUIStatus.click();
  yield promisePreferencesOpened
  yield signOut();
});
*/

add_task(function* test_verifiedUserEmptyProfile() {
  let promiseUpdateDone = promiseObserver("test:browser_fxaccounts:updateAppMenuItem", 1);
  yield setSignedInUser(true); // this will fire the observer that does the update.
  yield promiseUpdateDone;

  // Check the world.
  Assert.ok(isFooterVisible())
  Assert.equal(panelUILabel.getAttribute("label"), "foo@example.com");
  Assert.equal(panelUIStatus.getAttribute("tooltiptext"),
               panelUIStatus.getAttribute("signedinTooltiptext"));
  Assert.equal(panelUIFooter.getAttribute("fxastatus"), "signedin");

  let promisePreferencesOpened = promiseObserver("test:browser_fxaccounts:openPreferences");
  panelUIStatus.click();
  yield promisePreferencesOpened;
  yield signOut();
});

// Helpers.
function isFooterVisible() {
  let style = window.getComputedStyle(panelUIFooter);
  return style.getPropertyValue("display") == "flex";
}

function configureProfileURL(profile, responseStatus = 200) {
  let responseBody = profile ? JSON.stringify(profile) : "";
  let url = TEST_ROOT + "fxa_profile_handler.sjs?" +
            "responseStatus=" + responseStatus +
            "responseBody=" + responseBody +
            // This is a bit cheeky - the FxA code will just append "/profile"
            // to the preference value. We arrange for this to be seen by our
            //.sjs as part of the query string.
            "&path=";

  Services.prefs.setCharPref("identity.fxaccounts.remote.profile.uri", url);
}

function promiseObserver(topic, count = 1) {
  return new Promise(resolve => {
    let obs = (subject, topic, data) => {
      if (--count == 0) {
        Services.obs.removeObserver(obs, topic);
        resolve(subject);
      }
    }
    Services.obs.addObserver(obs, topic, false);
  });
}

// Stolen from browser_aboutHome.js
function promiseWaitForEvent(node, type, capturing) {
  return new Promise((resolve) => {
    node.addEventListener(type, function listener(event) {
      node.removeEventListener(type, listener, capturing);
      resolve(event);
    }, capturing);
  });
}

let promiseTabOpen = Task.async(function*(urlBase) {
  info("Waiting for tab to open...");
  let event = yield promiseWaitForEvent(gBrowser.tabContainer, "TabOpen", true);
  let tab = event.target;
  yield promiseTabLoadEvent(tab);
  ok(tab.linkedBrowser.currentURI.spec.startsWith(urlBase),
     "Got " + tab.linkedBrowser.currentURI.spec + ", expecting " + urlBase);
  let whenUnloaded = promiseTabUnloaded(tab);
  gBrowser.removeTab(tab);
  yield whenUnloaded;
});

function promiseTabUnloaded(tab)
{
  return new Promise(resolve => {
    info("Wait for tab to unload");
    function handle(event) {
      tab.linkedBrowser.removeEventListener("unload", handle, true);
      info("Got unload event");
      resolve(event);
    }
    tab.linkedBrowser.addEventListener("unload", handle, true, true);
  });
}

// FxAccounts helpers.
function setSignedInUser(verified) {
  let data = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    assertion: "foobar",
    sessionToken: "dead",
    kA: "beef",
    kB: "cafe",
    verified: verified,

    oauthTokens: {
      // a token for the profile server.
      profile: "key value",
    }
  }
  return fxAccounts.setSignedInUser(data);
}

let signOut = Task.async(function* () {
  // This test needs to make sure that any updates for the logout have
  // completed before starting the next test, or we see the observer
  // notifications get out of sync.
  let promiseUpdateDone = promiseObserver("test:browser_fxaccounts:updateAppMenuItem");
  // we always want a "localOnly" signout here...
  yield fxAccounts.signOut(true);
  yield promiseUpdateDone;
});
