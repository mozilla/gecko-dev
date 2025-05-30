/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

/**
 * Element used for displaying an avatar on the about:editprofile and about:newprofile pages.
 * profiles-group-item wraps this element to behave as a radio element.
 */
export class ProfileAvatarSelector extends MozLitElement {
  static properties = {
    value: { type: String },
  };

  getAvatarL10nId(value) {
    switch (value) {
      case "book":
        return "book-avatar";
      case "briefcase":
        return "briefcase-avatar";
      case "flower":
        return "flower-avatar";
      case "heart":
        return "heart-avatar";
      case "shopping":
        return "shopping-avatar";
      case "star":
        return "star-avatar";
    }

    return "";
  }

  iconTabContent() {
    let avatars = [
      "star",
      "flower",
      "briefcase",
      "heart",
      "book",
      "shopping",
      "present",
      "plane",
      "barbell",
      "bike",
      "craft",
      "diamond",
      "hammer",
      "heart-rate",
      "leaf",
      "makeup",
      "palette",
      "musical-note",
      "paw-print",
      "sparkle-single",
      "soccer",
      "video-game-controller",
      "default-favicon",
      "canvas",
      "history",
      "folder",
      "message",
      "lightbulb",
    ];

    // TODO: Bug 1966951 should remove the line below.
    // The browser_custom_avatar_test.js test will crash because the icon
    // files don't exist.
    avatars = avatars.slice(0, 6);

    return html`<profiles-group
      value=${this.avatar}
      name="avatar"
      id="avatars"
      @click=${this.handleAvatarClick}
      >${avatars.map(
        avatar =>
          html`<profiles-group-item
            l10nId=${this.getAvatarL10nId(avatar)}
            value=${avatar}
            ><moz-button
              class="avatar-button"
              type="ghost"
              iconSrc="chrome://browser/content/profiles/assets/16_${avatar}.svg"
            ></moz-button
          ></profiles-group-item>`
      )}</profiles-group
    >`;
  }

  render() {
    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/profiles/profile-avatar-selector.css"
      />
      <moz-card>
        <div class="button-group">
          <moz-button
            type="primary"
            data-l10n-id="avatar-selector-icon-tab"
          ></moz-button
          ><moz-button data-l10n-id="avatar-selector-custom-tab"></moz-button>
        </div>
        ${this.iconTabContent()}
      </moz-card>`;
  }
}

customElements.define("profile-avatar-selector", ProfileAvatarSelector);
