/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrefUtils: "resource://normandy/lib/PrefUtils.sys.mjs",
});
XPCOMUtils.defineLazyPreferenceGetter(lazy, "sidebarNimbus", "sidebar.nimbus");

export const SidebarManager = {
  /**
   * Handle startup tasks like telemetry, adding listeners.
   */
  init() {
    // Handle nimbus feature pref setting updates on init and enrollment
    const featureId = "sidebar";
    lazy.NimbusFeatures[featureId].onUpdate(() => {
      // Set prefs only if we have an enrollment that's new
      const feature = { featureId };
      const enrollment =
        lazy.ExperimentAPI.getExperimentMetaData(feature) ??
        lazy.ExperimentAPI.getRolloutMetaData(feature);
      if (!enrollment) {
        return;
      }
      const slug = enrollment.slug + ":" + enrollment.branch.slug;
      if (slug == lazy.sidebarNimbus) {
        return;
      }

      // Set/override user prefs to persist after experiment end
      const setPref = (pref, value) => {
        // Only set prefs with a value (so no clearing)
        if (value != null) {
          lazy.PrefUtils.setPref("sidebar." + pref, value);
        }
      };
      setPref("nimbus", slug);
      ["main.tools", "revamp", "verticalTabs"].forEach(pref =>
        setPref(pref, lazy.NimbusFeatures[featureId].getVariable(pref))
      );
    });
  },
};

// Initialize on first import
SidebarManager.init();
