/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";

/**
 * Element used for displaying a theme on the about:editprofile and about:newprofile pages.
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

  updateThemeImage() {
    if (!this.theme) {
      return;
    }

    if (this.theme.id === "default-theme@mozilla.org") {
      // For system theme, we use a special SVG that shows the light/dark wave design
      this.backgroundImg.src =
        "chrome://browser/content/profiles/assets/system-theme-background.svg";
    } else {
      let contentColor;
      if (!this.theme.contentColor) {
        let styles = window.getComputedStyle(document.body);
        contentColor = styles.getPropertyValue("background-color");
      }

      // For other themes, use the standard SVG with dynamic colors
      this.backgroundImg.src =
        "chrome://browser/content/profiles/assets/theme-selector-background.svg";
      this.backgroundImg.style.fill = this.theme.chromeColor;
      this.backgroundImg.style.stroke = this.theme.toolbarColor;
      this.imgHolder.style.backgroundColor =
        this.theme.contentColor ?? contentColor;
    }
  }

  updated() {
    super.updated();
    this.updateThemeImage();
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
          <div class="img-holder" aria-hidden="true">
            <img alt="" />
          </div>
          <div
            class="theme-name"
            id=${this.theme.name}
            data-l10n-id=${ifDefined(this.theme.dataL10nId)}
          >
            ${this.theme.name}
          </div>
        </div>
      </moz-card>`;
  }
}

customElements.define("profiles-theme-card", ProfilesThemeCard);
