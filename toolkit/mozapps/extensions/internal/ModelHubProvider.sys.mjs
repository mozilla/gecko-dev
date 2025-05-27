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
  isAddonEngineId: "chrome://global/content/ml/Utils.sys.mjs",
  engineIdToAddonId: "chrome://global/content/ml/Utils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "LOCAL_MODEL_MANAGEMENT_ENABLED",
  "extensions.htmlaboutaddons.local_model_management",
  false,
  (_pref, _old, val) => ModelHubProvider[val ? "init" : "shutdown"]()
);

const MODELHUB_ADDON_ID_SUFFIX = "@modelhub.mozilla.org";
const MODELHUB_ADDON_TYPE = "mlmodel";

export class ModelHubAddonWrapper {
  #provider;
  id;
  model;
  version;
  totalSize;
  lastUsed;
  updateDate;
  modelIconURL;
  engineIds;

  constructor(params) {
    this.#provider = params.provider;
    this.id = params.id;
    this.model = params.model;
    this.version = params.version;
    this.totalSize = params.totalSize;
    this.lastUsed = params.lastUsed;
    this.updateDate = params.updateDate;
    this.modelIconURL = params.modelIconURL;
    this.engineIds = params.engineIds ?? [];
  }

  async uninstall() {
    await this.#provider.modelHub.deleteModels({
      model: this.model,
      revision: this.version,
      deletedBy: "about:addons",
    });

    await this.#provider.onUninstalled(this);
  }

  get usedByFirefoxFeatures() {
    return this.engineIds.filter(engineId => !lazy.isAddonEngineId(engineId));
  }

  get usedByAddonIds() {
    return this.engineIds
      .filter(engineId => lazy.isAddonEngineId(engineId))
      .map(engineId => lazy.engineIdToAddonId(engineId));
  }

  get name() {
    const parts = this.model.split("/");
    return parts.slice(2).join("/");
  }

  get modelHomepageURL() {
    // Model card URL for models downloaded from "model-hub.mozilla.org" should point to the corresponding "https://huggingface.co" url.
    return `https://${this.model}/`.replace(
      "https://model-hub.mozilla.org/",
      "https://huggingface.co/"
    );
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

  // TODO(Bug 1965104): replace extensionGeneric.svg with a new mlmodel-specific fallback icon
  get iconURL() {
    return (
      this.modelIconURL ||
      "chrome://mozapps/skin/extensions/extensionGeneric.svg"
    );
  }
}

export const ModelHubProvider = {
  cache: new Map(),
  modelHub: null,

  get name() {
    return "ModelHubProvider";
  },

  async getAddonsByTypes(types) {
    if (!lazy.LOCAL_MODEL_MANAGEMENT_ENABLED) {
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
    if (id?.endsWith(MODELHUB_ADDON_ID_SUFFIX) && !this.cache.size) {
      await this.refreshAddonCache();
    }
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

  init() {
    if (!lazy.LOCAL_MODEL_MANAGEMENT_ENABLED || this.initialized) {
      return;
    }

    this.initialized = true;
    lazy.AddonManagerPrivate.registerProvider(this, [MODELHUB_ADDON_TYPE]);
    this.modelHub = new lazy.ModelHub();
  },

  async onUninstalled(addon) {
    if (!this.cache.has(addon.id)) {
      return;
    }
    this.cache.delete(addon.id);
    lazy.AddonManagerPrivate.callAddonListeners("onUninstalled", addon);
  },

  clearAddonCache() {
    this.cache.clear();
  },

  getWrapperIdForModel(model) {
    return [
      lazy.computeSha256HashAsString(`${model.name}:${model.revision}`),
      MODELHUB_ADDON_ID_SUFFIX,
    ].join("");
  },

  async refreshAddonCache() {
    // Return earlier if the model hub provider was disabled.
    // by the time it was being called.
    if (!lazy.LOCAL_MODEL_MANAGEMENT_ENABLED) {
      return;
    }

    this.clearAddonCache();

    const models = await this.modelHub.listModels();

    for (const model of models) {
      const { metadata } = await this.modelHub.listFiles({
        model: model.name,
        revision: model.revision,
      });

      const modelIconURL = await this.modelHub.getOwnerIcon(model.name);
      const id = this.getWrapperIdForModel(model);

      const wrapper = new ModelHubAddonWrapper({
        provider: this,
        id,
        model: model.name,
        version: model.revision,
        totalSize: metadata.totalSize,
        lastUsed: new Date(metadata.lastUsed),
        updateDate: new Date(metadata.updateDate),
        modelIconURL,
        engineIds: metadata.engineIds,
      });
      this.cache.set(wrapper.id, wrapper);
    }
  },
};

Services.obs.addObserver(ModelHubProvider, "browser-delayed-startup-finished");
