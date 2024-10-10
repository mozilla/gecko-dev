/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

/**
 * Element used for selecting an avatar
 */
export class Avatar extends MozLitElement {
  static properties = {
    value: { type: String },
    selected: { type: Boolean, reflect: true },
  };

  render() {
    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/profiles/avatar.css"
      />
      <button type="button" class="avatar">
        <img
          src="chrome://browser/content/profiles/assets/48_${this.value}.svg"
        />
      </button>`;
  }
}

customElements.define("profiles-avatar", Avatar);
