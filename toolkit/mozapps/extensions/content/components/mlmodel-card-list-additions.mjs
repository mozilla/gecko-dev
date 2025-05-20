/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { html, until } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  featureEngineIdToFluentId: "chrome://global/content/ml/Utils.sys.mjs",
});

const DEFAULT_EXTENSION_ICON =
  "chrome://mozapps/skin/extensions/extensionGeneric.svg";

class MLModelCardListAdditions extends MozLitElement {
  static properties = {
    addon: { type: Object, reflect: false },
    expanded: { type: Boolean, reflect: true, attribute: true },
  };

  setAddon(addon) {
    this.addon = addon;
  }

  async renderUsedByAddons() {
    if (!this.addon.usedByAddonIds.length) {
      return null;
    }
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

  render() {
    if (this.addon?.type !== "mlmodel" || this.expanded) {
      return null;
    }
    return html`
      <link
        href="chrome://mozapps/content/extensions/components/mlmodel-card-list-additions.css"
        rel="stylesheet"
      />

      ${this.renderUsedByFirefoxFeatures()}
      ${until(this.renderUsedByAddons(), html``)}
    `;
  }
}

customElements.define("mlmodel-card-list-additions", MLModelCardListAdditions);
