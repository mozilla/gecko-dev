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

const CONFIRM_RESTART_PROMPT_RESTART_NOW = 0;

XPCOMUtils.defineLazyGetter(this, "jawsesrStrings", () =>
  Services.strings.createBundle("chrome://jaws-esr/locale/jaws-esr.properties"));
XPCOMUtils.defineLazyGetter(this, "brandBundle", () =>
  Services.strings.createBundle("chrome://branding/locale/brand.properties"));
XPCOMUtils.defineLazyGetter(this, "updates", () =>
  Services.strings.createBundle("chrome://mozapps/locale/update/updates.properties"));

const observer = {
  observe(subject, topic, data) {
    if (topic === "a11y-init-or-shutdown" && data === "1") {
      checkVersionPromptAndDisableE10S(); // eslint-disable-line no-use-before-define
    }
  }
};

function removeA11yInitOrShutdownObserver() {
  try {
    Services.obs.removeObserver(observer, A11Y_INIT_OR_SHUTDOWN);
  } catch (e) {
    // Accessibility init or shutdown event observer might not have been initialized if:
    // * A11y service was never used
    // * A11y service was enabled at startup
    // * Observer was already removed on a11y service init.
    console.log("Accessibility service init or shutdown observer does not exist.");
  }
}

function checkVersionPromptAndDisableE10S() {
  // User is assumed to not use OLDJAWS or e10s is already disabled.
  if (!Services.appinfo.shouldBlockIncompatJaws ||
      !Services.appinfo.browserTabsRemoteAutostart) {
    return;
  }

  removeA11yInitOrShutdownObserver();

  const brandShortName = brandBundle.GetStringFromName("brandShortName");
  const restartFirefoxText = updates.formatStringFromName("restartNowButton",
    [brandShortName], 1);
  const msg = jawsesrStrings.formatStringFromName("jawsesr.dialog.msg",
    [brandShortName, brandShortName], 2);

  let buttonFlags = (Services.prompt.BUTTON_POS_0 *
                     Services.prompt.BUTTON_TITLE_IS_STRING);
  buttonFlags += (Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_CANCEL);
  buttonFlags += Services.prompt.BUTTON_POS_0_DEFAULT;

  const buttonIndex = Services.prompt.confirmEx(null, restartFirefoxText, msg,
    buttonFlags, restartFirefoxText, null, null, null, {});

  if (buttonIndex === CONFIRM_RESTART_PROMPT_RESTART_NOW) {
    Services.prefs.setBoolPref(PREF_BROWSER_TABS_REMOTE_FORCE_DISABLE, true);
    Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
  }
}

function install() {}

function uninstall() {}

function startup() {
  // Do nothing if we are not on Windows or if e10s is disabled.
  if (AppConstants.platform !== "win" || !Services.appinfo.browserTabsRemoteAutostart) {
    return;
  }

  if (Services.appinfo.accessibilityEnabled) {
    checkVersionPromptAndDisableE10S();
  } else {
    Services.obs.addObserver(observer, A11Y_INIT_OR_SHUTDOWN);
  }
}

function shutdown() {
  removeA11yInitOrShutdownObserver();
}
