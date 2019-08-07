/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported install, uninstall, startup, shutdown */
/* eslint no-implicit-globals: "off" */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const A11Y_INIT_OR_SHUTDOWN = "a11y-init-or-shutdown";

const PREF_BROWSER_TABS_REMOTE_FORCE_DISABLE = "browser.tabs.remote.force-disable";
const PREF_ACCESSIBILITY_CLIENTS_OLDJAWS_TIMESTAMP = "accessibility.clients.oldjaws.timestamp";

const CONFIRM_RESTART_PROMPT_RESTART_NOW = 0;

const TIMESPAN_DAY = 24 * 60 * 60 * 1000;
let gTimer = null;

XPCOMUtils.defineLazyGetter(this, "jawsesrStrings", () =>
  Services.strings.createBundle("chrome://jaws-esr/locale/jaws-esr.properties"));
XPCOMUtils.defineLazyGetter(this, "brandBundle", () =>
  Services.strings.createBundle("chrome://branding/locale/brand.properties"));
XPCOMUtils.defineLazyGetter(this, "updates", () =>
  Services.strings.createBundle("chrome://mozapps/locale/update/updates.properties"));

const observer = {
  observe(subject, topic, data) {
    if (topic === "a11y-init-or-shutdown") {
      if (data === "1") {
        checkEnvironmentAndUpdateSettings(); // eslint-disable-line no-use-before-define
      } else if (gTimer) {
        // If there's a timer for a timestamp update, cancel it and update the
        // timestamp.
        gTimer.cancel();
        gTimer = null;
        updateTimestamp(); // eslint-disable-line no-use-before-define
      }
    }
  }
};

function updateTimestamp() {
  Services.prefs.setIntPref(PREF_ACCESSIBILITY_CLIENTS_OLDJAWS_TIMESTAMP,
    // We need to store seconds in order to fit within int prefs.
    Math.floor(Date.now() / 1000));
}

function setUpdateTimestampTimer() {
  gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  gTimer.initWithCallback(() => {
    gTimer = null;
    // If OLDJAWS is in use, update the timestamp and reset the timer.
    if (Services.appinfo.shouldBlockIncompatJaws) {
      updateTimestamp();
      setUpdateTimestampTimer();
    }
  }, TIMESPAN_DAY, Ci.nsITimer.TYPE_ONE_SHOT);
}

function promptAndToggleE10S(e10sOff) {
  const brandShortName = brandBundle.GetStringFromName("brandShortName");
  const restartFirefoxText = updates.formatStringFromName("restartNowButton",
    [brandShortName], 1);

  const msg = jawsesrStrings.formatStringFromName(
    e10sOff ? "jawsesr.dialog.msg" : "jawsesr.dialog.msg.e10sOn",
    [brandShortName, brandShortName], 2);

  let buttonFlags = (Services.prompt.BUTTON_POS_0 *
                     Services.prompt.BUTTON_TITLE_IS_STRING);
  buttonFlags += (Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_CANCEL);
  buttonFlags += Services.prompt.BUTTON_POS_0_DEFAULT;

  const buttonIndex = Services.prompt.confirmEx(null, restartFirefoxText, msg,
    buttonFlags, restartFirefoxText, null, null, null, {});

  if (buttonIndex === CONFIRM_RESTART_PROMPT_RESTART_NOW) {
    Services.prefs.setBoolPref(PREF_BROWSER_TABS_REMOTE_FORCE_DISABLE, e10sOff);
    Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
  }
}

function checkEnvironmentAndUpdateSettings() {
  if (Services.appinfo.shouldBlockIncompatJaws) {
    // Set a preference that sets the last time the OLDJAWS screen reader was
    // used. This will be used when deciding whether to update the users from
    // ESR60 to ESR68.
    updateTimestamp();
    setUpdateTimestampTimer();

    if (Services.appinfo.browserTabsRemoteAutostart) {
      // Broken configuration, suggest restarting the browser with e10s
      // disabled.
      promptAndToggleE10S(true);
    }

    // We are done, users with OLDJAWS will not be updated to ESR68 because of
    // the PREF_BROWSER_TABS_REMOTE_FORCE_DISABLE pref set to true.
    return;
  }

  // If there's a timer for a timestamp update, cancel it.
  if (gTimer) {
    gTimer.cancel();
    gTimer = null;
  }

  if (Services.appinfo.browserTabsRemoteAutostart) {
    // User using a client other than OLDJAWS with e10s enabled. This is supported
    // configuration, nothing to do here.
    return;
  }

  // User is using a non-OLDJAWS client with e10s disabled. Suggest enabling
  // e10s and restarting.
  promptAndToggleE10S(false);
}

function install() {}

function uninstall() {}

function startup() {
  // Do nothing if we are not on Windows.
  if (AppConstants.platform !== "win" || Cu.isInAutomation) {
    return;
  }

  if (Services.appinfo.accessibilityEnabled) {
    checkEnvironmentAndUpdateSettings();
  } else {
    Services.obs.addObserver(observer, A11Y_INIT_OR_SHUTDOWN);
  }
}

function shutdown() {
  // If there's a timer for a timestamp update, cancel it.
  if (gTimer) {
    gTimer.cancel();
    gTimer = null;
  }
  // If OLDJAWS is in use, set/update the timestamp.
  if (Services.appinfo.shouldBlockIncompatJaws) {
    updateTimestamp();
  }

  try {
    Services.obs.removeObserver(observer, A11Y_INIT_OR_SHUTDOWN);
  } catch (e) {
    // Accessibility init or shutdown event observer might not have been initialized if:
    // * A11y service was never used
    // * A11y service was enabled at startup
    // * Observer was already removed on a11y service init.
  }
}
