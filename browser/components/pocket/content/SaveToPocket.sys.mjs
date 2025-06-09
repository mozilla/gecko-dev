/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

function browserWindows() {
  return Services.wm.getEnumerator("navigator:browser");
}

export var SaveToPocket = {
  init() {
    // migrate enabled pref
    if (Services.prefs.prefHasUserValue("browser.pocket.enabled")) {
      Services.prefs.setBoolPref(
        "extensions.pocket.enabled",
        Services.prefs.getBoolPref("browser.pocket.enabled")
      );
      Services.prefs.clearUserPref("browser.pocket.enabled");
    }
    // Only define the pref getter now, so we don't get notified for the
    // migrated pref above.
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "prefEnabled",
      "extensions.pocket.enabled",
      true,
      this.onPrefChange.bind(this)
    );
    if (!this.prefEnabled) {
      // We avoid calling onPrefChange or similar here, because we don't want to
      // shut down things that haven't started up, or broadcast unnecessary messages.
      this.updateElements(false);
      Services.obs.addObserver(this, "browser-delayed-startup-finished");
    }
  },

  observe(subject, topic) {
    if (topic == "browser-delayed-startup-finished") {
      // We only get here if pocket is disabled; the observer is removed when
      // we're enabled.
      this.updateElementsInWindow(subject, false);
    }
  },

  onPrefChange(pref, oldValue, newValue) {
    if (!newValue) {
      Services.obs.addObserver(this, "browser-delayed-startup-finished");
    } else {
      Services.obs.removeObserver(this, "browser-delayed-startup-finished");
    }
    this.updateElements(newValue);
  },

  updateElements(enabled) {
    // loop through windows and show/hide all our elements.
    for (let win of browserWindows()) {
      this.updateElementsInWindow(win, enabled);
    }
  },

  updateElementsInWindow(win, enabled) {
    if (enabled) {
      win.document.documentElement.removeAttribute("pocketdisabled");
    } else {
      win.document.documentElement.setAttribute("pocketdisabled", "true");
    }
  },
};
