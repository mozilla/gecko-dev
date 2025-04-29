/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.sys.mjs",
  computeSha256HashAsString:
    "resource://gre/modules/addons/crypto-utils.sys.mjs",
  ModelHub: "chrome://global/content/ml/ModelHub.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "MODELHUB_PROVIDER_ENABLED",
  "browser.ml.modelHubProvider",
  false,
  (_pref, _old, val) => ModelHubProvider[val ? "init" : "shutdown"]()
);

const MODELHUB_ADDON_ID_SUFFIX = "@modelhub.mozilla.org";
const MODELHUB_ADDON_TYPE = "mlmodel";

export class ModelHubAddonWrapper {
  #provider;
  id;
  name;
  version;
  totalSize;
  lastUsed;
  updateDate;

  constructor(params) {
    this.#provider = params.provider;
    this.id = params.id;
    this.name = params.name;
    this.version = params.version;
    this.totalSize = params.totalSize;
    this.lastUsed = params.lastUsed;
    this.updateDate = params.updateDate;
  }

  async uninstall() {
    await this.#provider.modelHub.deleteModels({
      model: this.name,
      revision: this.version,
    });

    await this.#provider.onUninstalled(this);
  }

  get isActive() {
    return true;
  }

  get isCompatible() {
    return true;
  }

  get permissions() {
    return lazy.AddonManager.PERM_CAN_UNINSTALL;
  }

  get type() {
    return MODELHUB_ADDON_TYPE;
  }
}

export const ModelHubProvider = {
  cache: new Map(),
  modelHub: null,

  get name() {
    return "ModelHubProvider";
  },

  async getAddonsByTypes(types) {
    if (!lazy.MODELHUB_PROVIDER_ENABLED) {
      return [];
    }

    const match = types?.includes?.(MODELHUB_ADDON_TYPE);
    return match
      ? await this.refreshAddonCache().then(() =>
          Array.from(this.cache.values())
        )
      : [];
  },

  async getAddonByID(id) {
    return this.cache.get(id);
  },

  observe(_subject, topic, _data) {
    switch (topic) {
      case "browser-delayed-startup-finished":
        Services.obs.removeObserver(this, "browser-delayed-startup-finished");
        this.init();
        break;
    }
  },

  shutdown() {
    if (this.initialized) {
      lazy.AddonManagerPrivate.unregisterProvider(this);
      this.initialized = false;
      this.clearAddonCache();
    }
  },

  async init() {
    if (!lazy.MODELHUB_PROVIDER_ENABLED || this.initialized) {
      return;
    }

    this.initialized = true;
    lazy.AddonManagerPrivate.registerProvider(this, [MODELHUB_ADDON_TYPE]);
    this.modelHub = new lazy.ModelHub();
    await this.refreshAddonCache();
  },

  async onUninstalled(addon) {
    if (!this.cache.has(addon.id)) {
      return;
    }
    this.cache.delete(addon.id);
    lazy.AddonManagerPrivate.callAddonListeners("onUninstalled", addon);
  },

  async clearAddonCache() {
    this.cache.clear();
  },

  getWrapperIdForModel(model) {
    return [
      lazy.computeSha256HashAsString(`${model.name}:${model.revision}`),
      MODELHUB_ADDON_ID_SUFFIX,
    ].join("");
  },

  async refreshAddonCache() {
    const models = await this.modelHub.listModels();

    for (const model of models) {
      const { metadata } = await this.modelHub.listFiles({
        model: model.name,
        revision: model.revision,
      });

      const id = this.getWrapperIdForModel(model);

      const wrapper = new ModelHubAddonWrapper({
        provider: this,
        id,
        name: model.name,
        version: model.revision,
        totalSize: metadata.totalSize,
        lastUsed: new Date(metadata.lastUsed),
        updateDate: new Date(metadata.updateDate),
      });
      this.cache.set(wrapper.id, wrapper);
    }
  },
};

Services.obs.addObserver(ModelHubProvider, "browser-delayed-startup-finished");
