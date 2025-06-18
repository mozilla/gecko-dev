/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export function UrlClassifierExceptionListService() {}

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

const COLLECTION_NAME = "url-classifier-exceptions";

class Feature {
  constructor(name, prefName) {
    this.name = name;
    this.prefName = prefName;
    this.observers = new Set();
    this.prefValue = null;
    this.remoteEntries = null;

    if (prefName) {
      this.prefValue = Services.prefs.getStringPref(this.prefName, null);
      Services.prefs.addObserver(prefName, this);
    }
  }

  async addAndRunObserver(observer) {
    this.observers.add(observer);
    this.notifyObservers(observer);
  }

  removeObserver(observer) {
    this.observers.delete(observer);
  }

  observe(subject, topic, data) {
    if (topic != "nsPref:changed" || data != this.prefName) {
      console.error(`Unexpected event ${topic} with ${data}`);
      return;
    }

    this.prefValue = Services.prefs.getStringPref(this.prefName, null);
    this.notifyObservers();
  }

  onRemoteSettingsUpdate(entries) {
    this.remoteEntries = [];

    for (let jsEntry of entries) {
      let { classifierFeatures } = jsEntry;
      if (classifierFeatures.includes(this.name)) {
        let entry = Feature.rsObjectToEntry(jsEntry);
        if (entry) {
          this.remoteEntries.push(entry);
        }
      }
    }
  }

  /**
   * Convert a JS object from RemoteSettings to an nsIUrlClassifierExceptionListEntry.
   * @param {Object} rsObject - The JS object from RemoteSettings to convert.
   * @returns {nsIUrlClassifierExceptionListEntry} The converted nsIUrlClassifierExceptionListEntry.
   */
  static rsObjectToEntry(rsObject) {
    let entry = Cc[
      "@mozilla.org/url-classifier/exception-list-entry;1"
    ].createInstance(Ci.nsIUrlClassifierExceptionListEntry);

    let {
      category: categoryStr,
      urlPattern,
      topLevelUrlPattern = "",
      isPrivateBrowsingOnly = false,
      filterContentBlockingCategories = [],
      classifierFeatures = [],
    } = rsObject;

    const CATEGORY_STR_TO_ENUM = {
      "internal-pref":
        Ci.nsIUrlClassifierExceptionListEntry.CATEGORY_INTERNAL_PREF,
      baseline: Ci.nsIUrlClassifierExceptionListEntry.CATEGORY_BASELINE,
      convenience: Ci.nsIUrlClassifierExceptionListEntry.CATEGORY_CONVENIENCE,
    };

    let category = CATEGORY_STR_TO_ENUM[categoryStr];
    if (category == null) {
      console.error(
        "Invalid or unknown category",
        { rsObject },
        { categories: Object.keys(CATEGORY_STR_TO_ENUM) }
      );
      return null;
    }

    try {
      entry.init(
        category,
        urlPattern,
        topLevelUrlPattern,
        isPrivateBrowsingOnly,
        filterContentBlockingCategories,
        classifierFeatures
      );
    } catch (e) {
      console.error(
        "Error initializing url classifier exception list entry " + e.message,
        e,
        { rsObject }
      );
      return null;
    }

    return entry;
  }

  notifyObservers(observer = null) {
    let entries = [];
    if (this.prefValue) {
      for (let prefEntry of this.prefValue.split(",")) {
        let entry = Feature.rsObjectToEntry({
          category: "internal-pref",
          urlPattern: prefEntry,
          classifierFeatures: [this.name],
        });
        if (entry) {
          entries.push(entry);
        }
      }
    }

    if (this.remoteEntries) {
      for (let entry of this.remoteEntries) {
        entries.push(entry);
      }
    }

    // Construct nsIUrlClassifierExceptionList with all entries that belong to
    // this feature.
    let list = Cc[
      "@mozilla.org/url-classifier/exception-list;1"
    ].createInstance(Ci.nsIUrlClassifierExceptionList);
    for (let entry of entries) {
      try {
        list.addEntry(entry);
      } catch (e) {
        console.error(
          "Error adding url classifier exception list entry " + e.message,
          e,
          entry
        );
      }
    }

    if (observer) {
      observer.onExceptionListUpdate(list);
    } else {
      for (let obs of this.observers) {
        obs.onExceptionListUpdate(list);
      }
    }
  }
}

UrlClassifierExceptionListService.prototype = {
  classID: Components.ID("{b9f4fd03-9d87-4bfd-9958-85a821750ddc}"),
  QueryInterface: ChromeUtils.generateQI([
    "nsIUrlClassifierExceptionListService",
  ]),

  features: {},
  _initialized: false,

  async lazyInit() {
    if (this._initialized) {
      return;
    }

    let rs = lazy.RemoteSettings(COLLECTION_NAME);
    rs.on("sync", event => {
      let {
        data: { current },
      } = event;
      this.entries = current || [];
      this.onUpdateEntries(current);
    });

    this._initialized = true;

    // If the remote settings list hasn't been populated yet we have to make sure
    // to do it before firing the first notification.
    // This has to be run after _initialized is set because we'll be
    // blocked while getting entries from RemoteSetting, and we don't want
    // LazyInit is executed again.
    try {
      // The data will be initially available from the local DB (via a
      // resource:// URI).
      this.entries = await rs.get();
    } catch (e) {}

    // RemoteSettings.get() could return null, ensure passing a list to
    // onUpdateEntries.
    if (!this.entries) {
      this.entries = [];
    }

    this.onUpdateEntries(this.entries);
  },

  onUpdateEntries(entries) {
    for (let key of Object.keys(this.features)) {
      let feature = this.features[key];
      feature.onRemoteSettingsUpdate(entries);
      feature.notifyObservers();
    }
  },

  registerAndRunExceptionListObserver(feature, prefName, observer) {
    // We don't await this; the caller is C++ and won't await this function,
    // and because we prevent re-entering into this method, once it's been
    // called once any subsequent calls will early-return anyway - so
    // awaiting that would be meaningless. Instead, `Feature` implementations
    // make sure not to call into observers until they have data, and we
    // make sure to let feature instances know whether we have data
    // immediately.
    this.lazyInit();

    if (!this.features[feature]) {
      let featureObj = new Feature(feature, prefName);
      this.features[feature] = featureObj;
      // If we've previously initialized, we need to pass the entries
      // we already have to the new feature.
      if (this.entries) {
        featureObj.onRemoteSettingsUpdate(this.entries);
      }
    }
    this.features[feature].addAndRunObserver(observer);
  },

  unregisterExceptionListObserver(feature, observer) {
    if (!this.features[feature]) {
      return;
    }
    this.features[feature].removeObserver(observer);
  },

  clear() {
    this.features = {};
    this._initialized = false;
    this.entries = null;
  },
};
