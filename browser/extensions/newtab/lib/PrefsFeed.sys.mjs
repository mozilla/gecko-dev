/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  actionCreators as ac,
  actionTypes as at,
} from "resource://newtab/common/Actions.mjs";
import { Prefs } from "resource://newtab/lib/ActivityStreamPrefs.sys.mjs";

// We use importESModule here instead of static import so that
// the Karma test environment won't choke on this module. This
// is because the Karma test environment already stubs out
// AppConstants, and overrides importESModule to be a no-op (which
// can't be done for a static import statement).

// eslint-disable-next-line mozilla/use-static-import
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  Region: "resource://gre/modules/Region.sys.mjs",
});

export class PrefsFeed {
  constructor(prefMap) {
    this._prefMap = prefMap;
    this._prefs = new Prefs();
    this.onExperimentUpdated = this.onExperimentUpdated.bind(this);
    this.onPocketExperimentUpdated = this.onPocketExperimentUpdated.bind(this);
    this.onSmartShortcutsExperimentUpdated =
      this.onSmartShortcutsExperimentUpdated.bind(this);
  }

  onPrefChanged(name, value) {
    const prefItem = this._prefMap.get(name);
    if (prefItem) {
      let action = "BroadcastToContent";
      if (prefItem.skipBroadcast) {
        action = "OnlyToMain";
        if (prefItem.alsoToPreloaded) {
          action = "AlsoToPreloaded";
        }
      }

      this.store.dispatch(
        ac[action]({
          type: at.PREF_CHANGED,
          data: { name, value },
        })
      );
    }
  }

  _setStringPref(values, key, defaultValue) {
    this._setPref(values, key, defaultValue, Services.prefs.getStringPref);
  }

  _setBoolPref(values, key, defaultValue) {
    this._setPref(values, key, defaultValue, Services.prefs.getBoolPref);
  }

  _setIntPref(values, key, defaultValue) {
    this._setPref(values, key, defaultValue, Services.prefs.getIntPref);
  }

  _setPref(values, key, defaultValue, getPrefFunction) {
    let value = getPrefFunction(
      `browser.newtabpage.activity-stream.${key}`,
      defaultValue
    );
    values[key] = value;
    this._prefMap.set(key, { value });
  }

  /**
   * Handler for when experiment data updates.
   */
  onExperimentUpdated() {
    const value = lazy.NimbusFeatures.newtab.getAllVariables() || {};
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.PREF_CHANGED,
        data: {
          name: "featureConfig",
          value,
        },
      })
    );
  }

  /**
   * Handler for Pocket specific experiment data updates.
   */
  onPocketExperimentUpdated(event, reason) {
    const value = lazy.NimbusFeatures.pocketNewtab.getAllVariables() || {};
    // Loaded experiments are set up inside init()
    if (
      reason !== "feature-experiment-loaded" &&
      reason !== "feature-rollout-loaded"
    ) {
      this.store.dispatch(
        ac.BroadcastToContent({
          type: at.PREF_CHANGED,
          data: {
            name: "pocketConfig",
            value,
          },
        })
      );
    }
  }

  /**
   * Handler for when smart shortcuts experiment data updates.
   */
  onSmartShortcutsExperimentUpdated() {
    const value =
      lazy.NimbusFeatures.newtabSmartShortcuts.getAllVariables() || {};
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.PREF_CHANGED,
        data: {
          name: "smartShortcutsConfig",
          value,
        },
      })
    );
  }

  init() {
    this._prefs.observeBranch(this);
    lazy.NimbusFeatures.newtab.onUpdate(this.onExperimentUpdated);
    lazy.NimbusFeatures.pocketNewtab.onUpdate(this.onPocketExperimentUpdated);
    lazy.NimbusFeatures.newtabSmartShortcuts.onUpdate(
      this.onSmartShortcutsExperimentUpdated
    );

    // Get the initial value of each activity stream pref
    const values = {};
    for (const name of this._prefMap.keys()) {
      values[name] = this._prefs.get(name);
    }

    // These are not prefs, but are needed to determine stuff in content that can only be
    // computed in main process
    values.isPrivateBrowsingEnabled = lazy.PrivateBrowsingUtils.enabled;
    values.platform = AppConstants.platform;

    // Save the geo pref if we have it
    if (lazy.Region.home) {
      values.region = lazy.Region.home;
      this.geo = values.region;
    } else if (this.geo !== "") {
      // Watch for geo changes and use a dummy value for now
      Services.obs.addObserver(this, lazy.Region.REGION_TOPIC);
      this.geo = "";
    }

    // Get the firefox accounts url for links and to send firstrun metrics to.
    values.fxa_endpoint = Services.prefs.getStringPref(
      "browser.newtabpage.activity-stream.fxaccounts.endpoint",
      "https://accounts.firefox.com"
    );

    // Get the firefox update channel with values as default, nightly, beta or release
    values.appUpdateChannel = Services.prefs.getStringPref(
      "app.update.channel",
      ""
    );

    // Read the pref for search shortcuts top sites experiment from firefox.js and store it
    // in our internal list of prefs to watch
    let searchTopSiteExperimentPrefValue = Services.prefs.getBoolPref(
      "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts"
    );
    values["improvesearch.topSiteSearchShortcuts"] =
      searchTopSiteExperimentPrefValue;
    this._prefMap.set("improvesearch.topSiteSearchShortcuts", {
      value: searchTopSiteExperimentPrefValue,
    });

    values.mayHaveSponsoredTopSites = Services.prefs.getBoolPref(
      "browser.topsites.useRemoteSetting"
    );

    // Read the pref for search hand-off from firefox.js and store it
    // in our internal list of prefs to watch
    let handoffToAwesomebarPrefValue = Services.prefs.getBoolPref(
      "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar"
    );
    values["improvesearch.handoffToAwesomebar"] = handoffToAwesomebarPrefValue;
    this._prefMap.set("improvesearch.handoffToAwesomebar", {
      value: handoffToAwesomebarPrefValue,
    });

    // Add experiment values and default values
    values.featureConfig = lazy.NimbusFeatures.newtab.getAllVariables() || {};
    values.pocketConfig =
      lazy.NimbusFeatures.pocketNewtab.getAllVariables() || {};
    values.smartShortcutsConfig =
      lazy.NimbusFeatures.newtabSmartShortcuts.getAllVariables() || {};
    this._setBoolPref(values, "logowordmark.alwaysVisible", false);
    this._setBoolPref(values, "feeds.section.topstories", false);
    this._setBoolPref(values, "discoverystream.enabled", false);
    this._setBoolPref(
      values,
      "discoverystream.sponsored-collections.enabled",
      false
    );
    this._setBoolPref(values, "discoverystream.isCollectionDismissible", false);
    this._setBoolPref(values, "discoverystream.hardcoded-basic-layout", false);
    this._setBoolPref(values, "discoverystream.personalization.enabled", false);
    this._setBoolPref(values, "discoverystream.personalization.override");
    this._setStringPref(
      values,
      "discoverystream.personalization.modelKeys",
      ""
    );
    this._setStringPref(values, "discoverystream.spocs-endpoint", "");
    this._setStringPref(values, "discoverystream.spocs-endpoint-query", "");
    this._setStringPref(values, "newNewtabExperience.colors", "");

    // Set the initial state of all prefs in redux
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.PREFS_INITIAL_VALUES,
        data: values,
        meta: {
          isStartup: true,
        },
      })
    );
  }

  uninit() {
    this.removeListeners();
  }

  removeListeners() {
    this._prefs.ignoreBranch(this);
    lazy.NimbusFeatures.newtab.offUpdate(this.onExperimentUpdated);
    lazy.NimbusFeatures.pocketNewtab.offUpdate(this.onPocketExperimentUpdated);
    lazy.NimbusFeatures.newtabSmartShortcuts.offUpdate(
      this.onSmartShortcutsExperimentUpdated
    );
    if (this.geo === "") {
      Services.obs.removeObserver(this, lazy.Region.REGION_TOPIC);
    }
  }

  observe(subject, topic) {
    switch (topic) {
      case lazy.Region.REGION_TOPIC:
        this.store.dispatch(
          ac.BroadcastToContent({
            type: at.PREF_CHANGED,
            data: { name: "region", value: lazy.Region.home },
          })
        );
        break;
    }
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.init();
        break;
      case at.UNINIT:
        this.uninit();
        break;
      case at.CLEAR_PREF:
        Services.prefs.clearUserPref(this._prefs._branchStr + action.data.name);
        break;
      case at.SET_PREF:
        this._prefs.set(action.data.name, action.data.value);
        break;
    }
  }
}
