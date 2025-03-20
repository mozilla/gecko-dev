/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "MODELHUB_PROVIDER_ENABLED",
  "browser.ml.modelHubProvider",
  false,
  (_pref, _old, val) => ModelHubProvider[val ? "init" : "uninit"]()
);

const MODELHUB_ADDON_ID_SUFFIX = "-modelhub@mozilla.org";
const MODELHUB_ADDON_TYPE = "mlmodel";

class ModelHubAddonWrapper {
  constructor(id = MODELHUB_ADDON_ID_SUFFIX) {
    this.id = id;
  }

  get isActive() {
    return true;
  }
  get isCompatible() {
    return true;
  }
  get name() {
    return "ModelHub";
  }
  get permissions() {
    return lazy.AddonManager.PERM_CAN_UNINSTALL;
  }
  get type() {
    return MODELHUB_ADDON_TYPE;
  }
}

const ModelHubProvider = {
  get name() {
    return "ModelHubProvider";
  },

  init() {
    // Activate lazy getter and initialize if necessary
    if (lazy.MODELHUB_PROVIDER_ENABLED && !this.initialized) {
      this.initialized = true;
      lazy.AddonManagerPrivate.registerProvider(this, [MODELHUB_ADDON_TYPE]);
    }
  },

  uninit() {
    if (this.initialized) {
      lazy.AddonManagerPrivate.unregisterProvider(this);
      this.initialized = false;
    }
  },

  observe(_subject, topic, _data) {
    switch (topic) {
      case "browser-delayed-startup-finished":
        Services.obs.removeObserver(this, "browser-delayed-startup-finished");
        this.init();
        break;
    }
  },

  async getAddonByID(id) {
    // TODO: should return a ModelHubAddonWrapper only if we have it
    return id == MODELHUB_ADDON_ID_SUFFIX ? new ModelHubAddonWrapper() : null;
  },

  async getAddonsByTypes(types) {
    // TODO: should return ModelHubAddonWrappers only if we have them
    return !types || types.includes(MODELHUB_ADDON_TYPE)
      ? [new ModelHubAddonWrapper()]
      : [];
  },
};

Services.obs.addObserver(ModelHubProvider, "browser-delayed-startup-finished");
