/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env mozilla/browser-window */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

/**
 * Helper for determining whether we're in the process of tearing down a
 * private browsing session. Teardown is started when closing of the last
 * private browsing window and stops once the last private browsing Browsing
 * Context has been discarded.
 * If a new private browsing window is opened during teardown telemetry is
 * collected.
 */
export const PBMExitStatus = {
  _isInitialized: false,
  // Keeps track of whether a PBM session is currently exiting.
  _isExiting: false,

  /**
   * Initialize the helper. Needs to be called before isExiting.
   */
  init() {
    // Init only once.
    if (this._isInitialized) {
      return;
    }
    this._isInitialized = true;

    Services.obs.addObserver(this, "last-pb-context-exiting-granted");
    Services.obs.addObserver(this, "last-pb-context-exited");

    Services.ww.registerNotification(this);
  },

  /**
   * Handler for PBM exited message.
   */
  _handlePrivateBrowsingExitStop() {
    // If we didn't see an exit start message don't record anything. This is for
    // skipping features like the PBM reset button which directly dispatches
    // "last-pb-context-exited" without all PBM windows being closed.
    if (!this._isExiting) {
      return;
    }
    this._isExiting = false;

    // Increase counter for total private browsing sessions.
    Glean.privateBrowsing.windowOpenDuringTeardown.addToDenominator(1);
  },

  /**
   * Handler for new chrome windows opened. Collects telemetry if we hit the PBM
   * teardown race condition.
   * @param {ChromeWindow} subject - The Chrome window that has been opened.
   */
  _handleDomWindowOpened(subject) {
    if (
      !this._isExiting ||
      !lazy.PrivateBrowsingUtils.isWindowPrivate(subject)
    ) {
      return;
    }
    // Set exiting flag to false. We don't expect to see a
    // "last-pb-context-exited" message now that a new PBM window has been
    // opened. The PBM session continues.
    this._isExiting = false;
    // Private browsing window opened while we're exiting private browsing.
    // Increase counter for race condition.
    Glean.privateBrowsing.windowOpenDuringTeardown.addToNumerator(1);
  },

  observe(subject, topic) {
    if (topic == "last-pb-context-exiting-granted") {
      this._isExiting = true;
    } else if (topic == "last-pb-context-exited") {
      this._handlePrivateBrowsingExitStop();
    } else if (topic == "domwindowopened") {
      this._handleDomWindowOpened(subject);
    }
  },
};
