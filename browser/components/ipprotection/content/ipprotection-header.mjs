/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

export default class IPProtectionHeaderElement extends MozLitElement {
  static queries = {
    titleEl: "#ipprotection-header-title",
  };

  static properties = {
    titleId: { type: String },
  };

  constructor() {
    super();
    this.titleId = "";
  }

  connectedCallback() {
    super.connectedCallback();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/ipprotection/ipprotection-header.css"
      />
      <div id="ipprotection-header-wrapper">
        <!-- Beta tag element -->
        <h1>
          <span
            id="ipprotection-header-title"
            data-l10n-id=${this.titleId}
          ></span>
        </h1>
        <!-- Question mark info element -->
      </div>
    `;
  }
}

customElements.define("ipprotection-header", IPProtectionHeaderElement);
