/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

export default class IPProtectionPanelElement extends MozLitElement {
  static queries = {};

  static properties = {
    state: { type: Object },
  };

  constructor() {
    super();

    this.state = {};
  }

  connectedCallback() {
    super.connectedCallback();
    this.dispatchEvent(new CustomEvent("IPProtection:Init", { bubbles: true }));
  }

  disconnectedCallback() {
    super.disconnectedCallback();
  }

  render() {
    return html``;
  }
}

customElements.define("ipprotection-panel", IPProtectionPanelElement);
