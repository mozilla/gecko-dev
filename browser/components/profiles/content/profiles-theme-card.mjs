/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

/**
 * Element used for displaying a theme on the about:editprofile and about:newprofile pages.
 * profiles-group-item wraps this element to behave as a radio element.
 */
export class ProfilesThemeCard extends MozLitElement {
  static properties = {
    theme: { type: Object },
    value: { type: String },
  };

  static queries = {
    backgroundImg: "img",
    imgHolder: ".img-holder",
  };

  updated() {
    super.updated();

    this.backgroundImg.style.fill = this.theme.chromeColor;
    this.backgroundImg.style.stroke = this.theme.toolbarColor;
    this.imgHolder.style.backgroundColor = this.theme.contentColor;
  }

  render() {
    if (!this.theme) {
      return null;
    }

    // We're using the theme's `dataL10nId` to serve as a
    // unique ID to use with `aria-labelledby`.
    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/profiles/profiles-theme-card.css"
      />
      <moz-card class="theme-card">
        <div class="theme-content">
          <div class="img-holder">
            <img
              src="chrome://browser/content/profiles/assets/theme-selector-background.svg"
            />
          </div>
          <div
            class="theme-name"
            id=${this.theme.dataL10nId}
            data-l10n-id=${this.theme.dataL10nId}
          ></div>
        </div>
      </moz-card>`;
  }
}

customElements.define("profiles-theme-card", ProfilesThemeCard);
