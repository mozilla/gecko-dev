/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/GeckoViewChildModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
});

// This needs to match GeckoSessionSettings.java
const USER_AGENT_MODE_MOBILE = 0;
const USER_AGENT_MODE_DESKTOP = 1;

// Handles GeckoView content settings including:
// * tracking protection
// * user agent mode
class GeckoViewSettingsChild extends GeckoViewChildModule {
  onInit() {
    debug `onInit`;
    this._userAgentMode = USER_AGENT_MODE_MOBILE;
    this._userAgentOverride = null;
  }

  onSettingsUpdate() {
    debug `onSettingsUpdate ${this.settings}`;

    this.displayMode = this.settings.displayMode;
    this.useTrackingProtection = !!this.settings.useTrackingProtection;
    this.userAgentMode = this.settings.userAgentMode;
    this.userAgentOverride = this.settings.userAgentOverride;
    this.allowJavascript = this.settings.allowJavascript;
  }

  get useTrackingProtection() {
    return docShell.useTrackingProtection;
  }

  set useTrackingProtection(aUse) {
    docShell.useTrackingProtection = aUse;
  }

  get userAgentMode() {
    return this._userAgentMode;
  }

  set userAgentMode(aMode) {
    if (this.userAgentMode === aMode) {
      return;
    }
    this._userAgentMode = aMode;
    if (this._userAgentOverride !== null) {
      return;
    }
    const utils = content.windowUtils;
    utils.setDesktopModeViewport(aMode === USER_AGENT_MODE_DESKTOP);
  }

  get userAgentOverride() {
    return this._userAgentOverride;
  }

  set userAgentOverride(aUserAgent) {
    if (aUserAgent === this._userAgentOverride) {
      return;
    }
    this._userAgentOverride = aUserAgent;
    const utils = content.windowUtils;
    if (aUserAgent === null) {
      utils.setDesktopModeViewport(this._userAgentMode === USER_AGENT_MODE_DESKTOP);
      return;
    }
    utils.setDesktopModeViewport(false);
  }

  get displayMode() {
    const docShell = content && GeckoViewUtils.getRootDocShell(content);
    return docShell ? docShell.displayMode
                    : Ci.nsIDocShell.DISPLAY_MODE_BROWSER;
  }

  set displayMode(aMode) {
    const docShell = content && GeckoViewUtils.getRootDocShell(content);
    if (docShell) {
      docShell.displayMode = aMode;
    }
  }

  get allowJavascript() {
    return docShell.allowJavascript;
  }

  set allowJavascript(aAllowJavascript) {
    docShell.allowJavascript = aAllowJavascript;
  }
}

let {debug, warn} = GeckoViewSettingsChild.initLogging("GeckoViewSettings");
let module = GeckoViewSettingsChild.create(this);
