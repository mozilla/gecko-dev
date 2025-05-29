/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from /toolkit/content/preferencesBindings.js */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.importESModule(
    "resource://gre/modules/FxAccounts.sys.mjs"
  ).getFxAccountsSingleton();
});

Preferences.addAll([
  { id: "services.sync.engine.addons", type: "bool" },
  { id: "services.sync.engine.bookmarks", type: "bool" },
  { id: "services.sync.engine.history", type: "bool" },
  { id: "services.sync.engine.tabs", type: "bool" },
  { id: "services.sync.engine.prefs", type: "bool" },
  { id: "services.sync.engine.passwords", type: "bool" },
  { id: "services.sync.engine.addresses", type: "bool" },
  { id: "services.sync.engine.creditcards", type: "bool" },
]);

let gSyncChooseWhatToSync = {
  init() {
    this._setupEventListeners();
    this._adjustForPrefs();
    let options = window.arguments[0];
    if (options.disconnectFun) {
      // Offer 'Disconnect' functionality if it was provided
      document.addEventListener("dialogextra2", function () {
        options.disconnectFun().then(disconnected => {
          if (disconnected) {
            window.close();
          }
        });
      });
    } else {
      // Hide the 'Disconnect' button if not applicable
      document.getElementById("syncChooseOptions").getButton("extra2").hidden =
        true;
    }
  },

  // make whatever tweaks we need based on preferences.
  _adjustForPrefs() {
    // These 2 engines are unique in that there are prefs that make the
    // entire engine unavailable (which is distinct from "disabled").
    let enginePrefs = [
      ["services.sync.engine.addresses", ".sync-engine-addresses"],
      ["services.sync.engine.creditcards", ".sync-engine-creditcards"],
    ];
    for (let [enabledPref, className] of enginePrefs) {
      let availablePref = enabledPref + ".available";
      // If the engine is enabled we force it to be available, otherwise we see
      // spooky things happen (like it magically re-appear later)
      if (Services.prefs.getBoolPref(enabledPref, false)) {
        Services.prefs.setBoolPref(availablePref, true);
      }
      if (!Services.prefs.getBoolPref(availablePref)) {
        let elt = document.querySelector(className);
        elt.hidden = true;
      }
    }
  },
  _setupEventListeners() {
    document.addEventListener("dialogaccept", () => {
      // Record when the user saves sync settings.
      let settings = this._getSyncEngineEnablementChanges();
      lazy.fxAccounts.telemetry.recordSaveSyncSettings(settings).catch(err => {
        console.error("Failed to record save sync settings event", err);
      });
    });
  },
  _getSyncEngineEnablementChanges() {
    let engines = [
      "addons",
      "bookmarks",
      "history",
      "tabs",
      "prefs",
      "passwords",
      "addresses",
      "creditcards",
    ];
    let settings = {
      enabledEngines: [],
      disabledEngines: [],
    };

    for (let engine of engines) {
      let enabledPref = "services.sync.engine." + engine;
      let checkboxId = "syncEngine" + engine[0].toUpperCase() + engine.slice(1);
      let checkboxValue = document.getElementById(checkboxId).checked;

      // Check if the engine's stored pref value is the same as the engine's
      // checkbox value in the choose what to sync menu. If they aren't equal
      // and the checkbox is checked, we add the engine to the enabled list; if
      // the checkbox isn't checked we add the engine to the disabled list. If
      // the pref and the checkbox value are equal we do nothing as nothing was
      // changed.
      if (Services.prefs.getBoolPref(enabledPref, false) !== checkboxValue) {
        if (checkboxValue === true) {
          settings.enabledEngines.push(engine);
        } else if (checkboxValue === false) {
          settings.disabledEngines.push(engine);
        }
      }
    }
    return settings;
  },
};

window.addEventListener("load", () => gSyncChooseWhatToSync.init());
