/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export class AddonMLModelDetails extends MozLitElement {
  static properties = {
    addon: {
      type: Object,
      // Prevent this property from being converted into an DOM attribute
      // (LitElement would hit a `TypeError: cyclic object value` in tests using
      // MockProvider).
      reflect: false,
    },
  };

  // NOTE: opt-out from using the shadow dom as render root (and use the element
  // itself to host the custom element content instead).
  createRenderRoot() {
    return this;
  }

  render() {
    return this.template;
  }

  get template() {
    return html`
      <div class="addon-detail-row addon-detail-row-mlmodel-filesize">
        <label data-l10n-id="addon-detail-file-size-label"></label>
        ${this.addon?.fileSize}
      </div>
    `;
  }
}
customElements.define("addon-mlmodel-details", AddonMLModelDetails);
