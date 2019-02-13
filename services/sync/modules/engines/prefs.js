/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ['PrefsEngine', 'PrefRec'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const WEAVE_SYNC_PREFS = "services.sync.prefs.sync.";

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/LightweightThemeManager.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

const PREFS_GUID = CommonUtils.encodeBase64URL(Services.appinfo.ID);

this.PrefRec = function PrefRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
PrefRec.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.Pref",
};

Utils.deferGetSet(PrefRec, "cleartext", ["value"]);


this.PrefsEngine = function PrefsEngine(service) {
  SyncEngine.call(this, "Prefs", service);
}
PrefsEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: PrefStore,
  _trackerObj: PrefTracker,
  _recordObj: PrefRec,
  version: 2,

  syncPriority: 1,

  getChangedIDs: function getChangedIDs() {
    // No need for a proper timestamp (no conflict resolution needed).
    let changedIDs = {};
    if (this._tracker.modified)
      changedIDs[PREFS_GUID] = 0;
    return changedIDs;
  },

  _wipeClient: function _wipeClient() {
    SyncEngine.prototype._wipeClient.call(this);
    this.justWiped = true;
  },

  _reconcile: function _reconcile(item) {
    // Apply the incoming item if we don't care about the local data
    if (this.justWiped) {
      this.justWiped = false;
      return true;
    }
    return SyncEngine.prototype._reconcile.call(this, item);
  }
};


function PrefStore(name, engine) {
  Store.call(this, name, engine);
  Svc.Obs.add("profile-before-change", function () {
    this.__prefs = null;
  }, this);
}
PrefStore.prototype = {
  __proto__: Store.prototype,

 __prefs: null,
  get _prefs() {
    if (!this.__prefs)
      this.__prefs = new Preferences();
    return this.__prefs;
  },

  _getSyncPrefs: function _getSyncPrefs() {
    let syncPrefs = Cc["@mozilla.org/preferences-service;1"]
                      .getService(Ci.nsIPrefService)
                      .getBranch(WEAVE_SYNC_PREFS)
                      .getChildList("", {});
    // Also sync preferences that determine which prefs get synced.
    return syncPrefs.concat(
      syncPrefs.map(function (pref) { return WEAVE_SYNC_PREFS + pref; }));
  },

  _isSynced: function _isSyncedPref(pref) {
    return (pref.indexOf(WEAVE_SYNC_PREFS) == 0)
            || this._prefs.get(WEAVE_SYNC_PREFS + pref, false);
  },

  _getAllPrefs: function () {
    let values = {};
    for each (let pref in this._getSyncPrefs()) {
      if (this._isSynced(pref)) {
        // Missing prefs get the null value.
        values[pref] = this._prefs.get(pref, null);
      }
    }
    return values;
  },

  _setAllPrefs: function PrefStore__setAllPrefs(values) {
    let selectedThemeIDPref = "lightweightThemes.selectedThemeID";
    let selectedThemeIDBefore = this._prefs.get(selectedThemeIDPref, null);

    for (let [pref, value] in Iterator(values)) {
      if (!this._isSynced(pref))
        continue;

      // Pref has gone missing, best we can do is reset it.
      if (value == null) {
        this._prefs.reset(pref);
        continue;
      }

      try {
        this._prefs.set(pref, value);
      } catch(ex) {
        this._log.trace("Failed to set pref: " + pref + ": " + ex);
      } 
    }

    // Notify the lightweight theme manager if the selected theme has changed.
    let selectedThemeIDAfter = this._prefs.get(selectedThemeIDPref, null);
    if (selectedThemeIDBefore != selectedThemeIDAfter) {
      // The currentTheme getter will reflect the theme with the new
      // selectedThemeID (if there is one).  Just reset it to itself
      let currentTheme = LightweightThemeManager.currentTheme;
      LightweightThemeManager.currentTheme = null;
      LightweightThemeManager.currentTheme = currentTheme;
    }
  },

  getAllIDs: function PrefStore_getAllIDs() {
    /* We store all prefs in just one WBO, with just one GUID */
    let allprefs = {};
    allprefs[PREFS_GUID] = true;
    return allprefs;
  },

  changeItemID: function PrefStore_changeItemID(oldID, newID) {
    this._log.trace("PrefStore GUID is constant!");
  },

  itemExists: function FormStore_itemExists(id) {
    return (id === PREFS_GUID);
  },

  createRecord: function createRecord(id, collection) {
    let record = new PrefRec(collection, id);

    if (id == PREFS_GUID) {
      record.value = this._getAllPrefs();
    } else {
      record.deleted = true;
    }

    return record;
  },

  create: function PrefStore_create(record) {
    this._log.trace("Ignoring create request");
  },

  remove: function PrefStore_remove(record) {
    this._log.trace("Ignoring remove request");
  },

  update: function PrefStore_update(record) {
    // Silently ignore pref updates that are for other apps.
    if (record.id != PREFS_GUID)
      return;

    this._log.trace("Received pref updates, applying...");
    this._setAllPrefs(record.value);
  },

  wipe: function PrefStore_wipe() {
    this._log.trace("Ignoring wipe request");
  }
};

function PrefTracker(name, engine) {
  Tracker.call(this, name, engine);
  Svc.Obs.add("profile-before-change", this);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);
}
PrefTracker.prototype = {
  __proto__: Tracker.prototype,

  get modified() {
    return Svc.Prefs.get("engine.prefs.modified", false);
  },
  set modified(value) {
    Svc.Prefs.set("engine.prefs.modified", value);
  },

  loadChangedIDs: function loadChangedIDs() {
    // Don't read changed IDs from disk at start up.
  },

  clearChangedIDs: function clearChangedIDs() {
    this.modified = false;
  },

 __prefs: null,
  get _prefs() {
    if (!this.__prefs) {
      this.__prefs = new Preferences();
    }
    return this.__prefs;
  },

  startTracking: function () {
    Services.prefs.addObserver("", this, false);
  },

  stopTracking: function () {
    this.__prefs = null;
    Services.prefs.removeObserver("", this);
  },

  observe: function (subject, topic, data) {
    Tracker.prototype.observe.call(this, subject, topic, data);

    switch (topic) {
      case "profile-before-change":
        this.stopTracking();
        break;
      case "nsPref:changed":
        // Trigger a sync for MULTI-DEVICE for a change that determines
        // which prefs are synced or a regular pref change.
        if (data.indexOf(WEAVE_SYNC_PREFS) == 0 ||
            this._prefs.get(WEAVE_SYNC_PREFS + data, false)) {
          this.score += SCORE_INCREMENT_XLARGE;
          this.modified = true;
          this._log.trace("Preference " + data + " changed");
        }
        break;
    }
  }
};
