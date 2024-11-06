/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

/**
 * Element used for selecting a theme on the about:editprofile page
 */
export class ProfilesThemeCard extends MozLitElement {
  static properties = {
    theme: { type: Object },
    selected: { type: Boolean, reflect: true },
  };

  static queries = {
    backgroundImg: "img",
    imgHolder: ".img-holder",
  };

  handleKeydown(event) {
    if (event.code === "Enter" || event.code === "Space") {
      this.dispatchEvent(
        new CustomEvent("click", { bubbles: true, composed: true })
      );
    }
  }

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

    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/profiles/profiles-theme-card.css"
      />
      <moz-card class="theme-card" tabindex="0" @keydown=${this.handleKeydown}>
        <div class="theme-content">
          <div class="img-holder">
            <img
              src="chrome://browser/content/profiles/assets/theme-selector-background.svg"
            />
          </div>
          <div class="theme-name" data-l10n-id="${this.theme.dataL10nId}"></div>
        </div>
      </moz-card>`;
  }
}

customElements.define("profiles-theme-card", ProfilesThemeCard);
