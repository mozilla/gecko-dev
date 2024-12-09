/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const BACKUP_STATE_PREF = "sidebar.backupState";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrefUtils: "resource://normandy/lib/PrefUtils.sys.mjs",
});
XPCOMUtils.defineLazyPreferenceGetter(lazy, "sidebarNimbus", "sidebar.nimbus");
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "sidebarBackupState",
  BACKUP_STATE_PREF
);

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

      // Enforce minimum version by skipping pref changes until Firefox restarts
      // with the appropriate version
      if (
        Services.vc.compare(
          // Support betas, e.g., 132.0b1, instead of MOZ_APP_VERSION
          AppConstants.MOZ_APP_VERSION_DISPLAY,
          // Check configured version or compare with unset handled as 0
          lazy.NimbusFeatures[featureId].getVariable("minVersion")
        ) < 0
      ) {
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
      ["main.tools", "revamp", "verticalTabs", "visibility"].forEach(pref =>
        setPref(pref, lazy.NimbusFeatures[featureId].getVariable(pref))
      );
    });
  },

  /**
   * Provide a system-level "backup" state to be stored for those using "Never
   * remember history" or "Clear history when browser closes".
   *
   * If it doesn't exist or isn't parsable, return `null`.
   *
   * @returns {object}
   */
  getBackupState() {
    try {
      return JSON.parse(lazy.sidebarBackupState);
    } catch (e) {
      Services.prefs.clearUserPref(BACKUP_STATE_PREF);
      return null;
    }
  },

  /**
   * Set the backup state.
   *
   * @param {object} state
   */
  setBackupState(state) {
    if (!state) {
      return;
    }
    Services.prefs.setStringPref(BACKUP_STATE_PREF, JSON.stringify(state));
  },
};

// Initialize on first import
SidebarManager.init();
