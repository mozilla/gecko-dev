/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/profiles/profile-card.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-card.mjs";

const { SelectableProfile } = ChromeUtils.importESModule(
  "resource:///modules/profiles/SelectableProfile.sys.mjs"
);

/**
 * Element used for displaying a SelectableProfile in the profile selector window
 */
export class ProfileCard extends MozLitElement {
  static properties = {
    profile: { type: SelectableProfile, reflect: true },
  };

  static queries = {
    backgroundImage: ".profile-background-image",
    avatarImage: ".profile-avatar",
  };

  firstUpdated() {
    super.firstUpdated();

    this.setBackgroundImage();
    this.setAvatarImage();
  }

  setBackgroundImage() {
    this.backgroundImage.style.backgroundImage = `url("chrome://browser/content/profiles/assets/profilesBackground${
      this.profile.id % 5
    }.svg")`;
    let { themeFg, themeBg } = this.profile.theme;
    this.backgroundImage.style.fill = themeBg;
    this.backgroundImage.style.stroke = themeFg;
  }

  setAvatarImage() {
    this.avatarImage.style.backgroundImage = `url("chrome://browser/content/profiles/assets/80_${this.profile.avatar}.svg")`;
    let { themeFg, themeBg } = this.profile.theme;
    this.avatarImage.style.fill = themeBg;
    this.avatarImage.style.stroke = themeFg;
  }

  launchProfile(url) {
    this.dispatchEvent(
      new CustomEvent("LaunchProfile", {
        bubbles: true,
        composed: true,
        detail: { profile: this.profile, url },
      })
    );
  }

  click() {
    this.handleClick();
  }

  handleClick() {
    this.launchProfile();
  }

  handleKeyDown(event) {
    if (event.code === "Enter" || event.code === "Space") {
      this.launchProfile();
    }
  }

  handleEditClick(event) {
    event.stopPropagation();
    this.launchProfile("about:editprofile");
  }

  handleDeleteClick(event) {
    event.stopPropagation();
    this.dispatchEvent(
      new CustomEvent("DeleteProfile", {
        bubbles: true,
        composed: true,
        detail: this.profile,
      })
    );
  }

  render() {
    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/profiles/profile-card.css"
      />
      <div
        data-l10n-id="profile-card"
        data-l10n-args=${JSON.stringify({ profileName: this.profile.name })}
        class="profile-card"
        role="button"
        tabindex="0"
        @click=${this.handleClick}
        @keydown=${this.handleKeyDown}
      >
        <div class="profile-background-container">
          <div class="profile-background-image"></div>
          <div class="profile-avatar"></div>
        </div>
        <div class="profile-details">
          <h3 class="text-truncated-ellipsis">${this.profile.name}</h3>
          <moz-button-group
            ><moz-button
              data-l10n-id="profile-card-edit-button"
              type="ghost"
              iconsrc="chrome://global/skin/icons/edit-outline.svg"
              @click=${this.handleEditClick}
            ></moz-button
            ><moz-button
              data-l10n-id="profile-card-delete-button"
              type="ghost"
              iconsrc="chrome://global/skin/icons/delete.svg"
              @click=${this.handleDeleteClick}
            ></moz-button
          ></moz-button-group>
        </div>
      </div>`;
  }
}

customElements.define("profile-card", ProfileCard);

/**
 * Element used for creating a new SelectableProfile in the profile selector window
 */
export class NewProfileCard extends MozLitElement {
  createProfile() {
    this.dispatchEvent(
      new CustomEvent("CreateProfile", {
        bubbles: true,
        composed: true,
      })
    );
  }

  click() {
    this.handleClick();
  }

  handleClick() {
    this.createProfile();
  }

  handleKeyDown(event) {
    if (event.code === "Enter" || event.code === "Space") {
      this.createProfile();
    }
  }

  render() {
    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/profiles/profile-card.css"
      />
      <div
        class="profile-card new-profile-card"
        role="button"
        tabindex="0"
        @click=${this.handleClick}
        @keydown=${this.handleKeyDown}
      >
        <div class="profile-background-container">
          <div class="profile-background-image"></div>
          <div class="profile-avatar"></div>
        </div>
        <div class="profile-details">
          <h3 data-l10n-id="profile-window-create-profile"></h3>
        </div>
      </div>`;
  }
}

customElements.define("new-profile-card", NewProfileCard);
