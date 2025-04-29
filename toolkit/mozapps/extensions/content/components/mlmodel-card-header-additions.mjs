/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  DownloadUtils: "resource://gre/modules/DownloadUtils.sys.mjs",
});

class MLModelCardHeaderAdditions extends MozLitElement {
  static properties = {
    addon: { type: Object, reflect: false },
    expanded: { type: Boolean, reflect: true, attribute: true },
  };

  // NOTE: opt-out from using the shadow dom as render root (and use the element
  // itself to host the custom element content instead).
  createRenderRoot() {
    return this;
  }

  setAddon(addon) {
    this.addon = addon;
  }

  render() {
    if (this.addon?.type !== "mlmodel") {
      return null;
    }
    return html`
      <link
        href="chrome://mozapps/content/extensions/components/mlmodel-card-header-additions.css"
        rel="stylesheet"
      />
      ${this.expanded
        ? html`<button
            class="mlmodel-remove-addon-button"
            action="remove"
            data-l10n-id="mlmodel-remove-addon-button"
          ></button>`
        : html`<span class="mlmodel-total-size-bubble"
            >${this.getModelSizeString(this.addon)}</span
          >`}
    `;
  }

  getModelSizeString(addon) {
    return lazy.DownloadUtils.getTransferTotal(addon.totalSize ?? 0);
  }
}

customElements.define(
  "mlmodel-card-header-additions",
  MLModelCardHeaderAdditions
);
