/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { html, until } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  DownloadUtils: "resource://gre/modules/DownloadUtils.sys.mjs",
  featureEngineIdToFluentId: "chrome://global/content/ml/Utils.sys.mjs",
  recordExtensionModelLinkTelemetry: "chrome://global/content/ml/Utils.sys.mjs",
});

const DEFAULT_EXTENSION_ICON =
  "chrome://mozapps/skin/extensions/extensionGeneric.svg";

export class AddonMLModelDetails extends MozLitElement {
  static properties = {
    addon: {
      type: Object,
      // Prevent this property from being converted into an DOM attribute
      // (LitElement would hit a `TypeError: cyclic object value` in tests using
      // MockProvider).
      reflect: false,
    },
    lastUsed: { type: String },
    modelSize: { type: String },
  };

  setAddon(addon) {
    this.addon = addon;

    this.lastUsed = addon.lastUsed.toLocaleDateString(undefined, {
      year: "numeric",
      month: "long",
      day: "numeric",
    });
    this.modelSize = lazy.DownloadUtils.getTransferTotal(
      this.addon.totalSize ?? 0
    );
  }

  // NOTE: opt-out from using the shadow dom as render root (and use the element
  // itself to host the custom element content instead).
  createRenderRoot() {
    return this;
  }

  async renderUsedByAddons() {
    const addons = await AddonManager.getAddonsByIDs(this.addon.usedByAddonIds);
    return addons?.map(addon => {
      if (!addon) {
        return null;
      }
      const iconURL = addon.iconURL ?? DEFAULT_EXTENSION_ICON;
      return html`
        <div class="mlmodel-used-by">
          <img src=${iconURL} width="18" height="18" />
          <label
            data-l10n-id="mlmodel-extension-label"
            data-l10n-args=${JSON.stringify({ extensionName: addon.name })}
          ></label>
        </div>
      `;
    });
  }

  renderUsedByFirefoxFeatures() {
    return this.addon.usedByFirefoxFeatures.map(engineId => {
      const fluentId = lazy.featureEngineIdToFluentId(engineId);
      if (!fluentId) {
        return null;
      }
      return html`
        <div class="mlmodel-used-by">
          <img
            src="chrome://branding/content/icon64.png"
            height="18"
            width="18"
          />
          <label data-l10n-id=${fluentId}></label>
        </div>
      `;
    });
  }

  handleCardLinkClick() {
    lazy.recordExtensionModelLinkTelemetry(this.addon);
  }

  render() {
    if (this.addon?.type !== "mlmodel") {
      return null;
    }

    return html`
      <div class="mlmodel-used-by-wrapper">
        ${this.renderUsedByFirefoxFeatures()}
        ${until(this.renderUsedByAddons(), html``)}
      </div>
      <div class="addon-detail-row addon-detail-row-mlmodel-totalsize">
        <label data-l10n-id="mlmodel-addon-detail-totalsize-label"></label>
        <span>${this.modelSize}</span>
      </div>

      <div class="addon-detail-row addon-detail-row-mlmodel-lastused">
        <label data-l10n-id="mlmodel-addon-detail-last-used-label"></label>
        <span>${this.lastUsed}</span>
      </div>

      <div class="addon-detail-row addon-detail-row-mlmodel-modelcard">
        <label data-l10n-id="mlmodel-addon-detail-model-card"></label>
        <a
          target="_blank"
          @click=${this.handleCardLinkClick}
          href=${this.addon.modelHomepageURL}
          data-l10n-id="mlmodel-addon-detail-model-card-link-label"
        ></a>
      </div>
    `;
  }
}
customElements.define("addon-mlmodel-details", AddonMLModelDetails);
