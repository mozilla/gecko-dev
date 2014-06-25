// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

let FlyoutPanelsUI = {
  _isInitialized: false,

  init: function() {
    if (this._isInitialized) {
      Cu.reportError("Attempted to initialize FlyoutPanelsUI more than once");
      return;
    }

    Cu.import("resource://gre/modules/XPCOMUtils.jsm");
    Cu.import("resource://gre/modules/Services.jsm");

    this._isInitialized = true;
    let scriptContexts = {};
    let scripts =
          [
            ['AboutFlyoutPanel', 'chrome://browser/content/flyoutpanels/AboutFlyoutPanel.js'],
            ['PrefsFlyoutPanel', 'chrome://browser/content/flyoutpanels/PrefsFlyoutPanel.js'],
            ['SearchFlyoutPanel', 'chrome://browser/content/flyoutpanels/SearchFlyoutPanel.js'],
#ifdef MOZ_SERVICES_SYNC
            ['SyncFlyoutPanel', 'chrome://browser/content/flyoutpanels/SyncFlyoutPanel.js'],
#endif
          ];

    scripts.forEach(function (aScript) {
      let [name, script] = aScript;
      XPCOMUtils.defineLazyGetter(FlyoutPanelsUI, name, function() {
        let sandbox = {};
        Services.scriptloader.loadSubScript(script, sandbox);
        sandbox[name].init();
        return sandbox[name];
      });
    });

    Services.obs.addObserver(this, "metro_viewstate_changed", false);
    window.addEventListener("TabOpen", this, false);
  },

  uninit: function () {
    Services.obs.removeObserver(this, "metro_viewstate_changed");
    window.removeEventListener("TabOpen", this, false);
  },

  show: function(aToShow) {
    if (!this[aToShow]) {
      throw("FlyoutPanelsUI asked to show '" + aToShow + "' which does not exist");
    }

    if (this._currentFlyout) {
      if (this._currentFlyout == this[aToShow]) {
        return;
      } else {
        this.hide();
      }
    }

    this._currentFlyout = this[aToShow];
    if (this._currentFlyout._show) {
      this._currentFlyout._show();
    } else {
      this._currentFlyout._topmostElement.show();
    }
    DialogUI.pushPopup(this, this._currentFlyout._topmostElement);
  },

  onBackButton: function() {
    if (this._currentFlyout._onBackButton) {
      this._currentFlyout._onBackButton();
    } else {
      this.hide();
      Services.metro.showSettingsFlyout();
    }
  },

  get isVisible() {
    return this._currentFlyout ? true : false;
  },

  handleEvent: function (aEvent) {
    switch (aEvent.type) {
      case "TabOpen":
        this.hide()
        break;
    }
  },

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "metro_viewstate_changed":
        if (aData == "snapped") {
          this.hide();
        }
        break;
    }
  },

  dispatchEvent: function(aEvent) {
    if (this._currentFlyout) {
      this._currentFlyout._topmostElement.dispatchEvent(aEvent);
    }
  },

  hide: function() {
    if (this._currentFlyout) {
      if (this._currentFlyout._hide) {
        this._currentFlyout._hide();
      } else {
        this._currentFlyout._topmostElement.hide();
      }
      DialogUI.popPopup(this);
      delete this._currentFlyout;
    }
  }
};
