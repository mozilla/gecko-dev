/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

/**
 * Element used for displaying an avatar on the about:editprofile and about:newprofile pages.
 */
export class Avatar extends MozLitElement {
  static properties = {
    value: { type: String },
  };

  get imageL10nId() {
    switch (this.value) {
      case "book":
        return "book-avatar-alt";
      case "briefcase":
        return "briefcase-avatar-alt";
      case "flower":
        return "flower-avatar-alt";
      case "heart":
        return "heart-avatar-alt";
      case "shopping":
        return "shopping-avatar-alt";
      case "star":
        return "star-avatar-alt";
    }

    return "";
  }

  render() {
    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/profiles/avatar.css"
      />
      <div class="avatar" aria-labelledby="avatar-img">
        <img
          id="avatar-img"
          data-l10n-id=${this.imageL10nId}
          src="chrome://browser/content/profiles/assets/48_${this.value}.svg"
        />
      </div>`;
  }
}

customElements.define("profiles-avatar", Avatar);
