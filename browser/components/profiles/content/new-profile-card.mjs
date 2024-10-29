/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { EditProfileCard } from "chrome://browser/content/profiles/edit-profile-card.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-support-link.mjs";

/**
 * Element used for updating a profile's name, theme, and avatar.
 */
export class NewProfileCard extends EditProfileCard {
  async init() {
    if (this.initialized) {
      return;
    }

    let { currentProfile, profiles, themes, isInAutomation } =
      await RPMSendQuery("Profiles:GetNewProfileContent");

    if (isInAutomation) {
      this.updateNameDebouncer.timeout = 50;
    }

    this.profile = currentProfile;
    this.profiles = profiles;
    this.themes = themes;

    this.setInitialInput();

    this.initialized = true;
  }

  async setInitialInput() {
    if (RPMGetBoolPref("browser.profiles.profile-name.updated", false)) {
      return;
    }

    await this.updateComplete;

    this.nameInput.value = "";
  }

  onDeleteClick() {
    RPMSendAsyncMessage("Profiles:DeleteNewProfile");
  }

  onDoneClick() {
    let newName = this.nameInput.value.trim();
    if (newName === "") {
      this.showErrorMessage("edit-profile-page-no-name");
    } else if (this.isDuplicateName(newName)) {
      this.showErrorMessage("edit-profile-page-duplicate-name");
    } else {
      this.updateName();
      RPMSendAsyncMessage("Profiles:CloseNewProfileTab");
    }
  }

  headerTemplate() {
    return html`<div>
      <h1 data-l10n-id="new-profile-page-header"></h1>
      <p>
        <span data-l10n-id="new-profile-page-header-description"></span>
        <a
          is="moz-support-link"
          support-page="profiles"
          data-l10n-id="new-profile-page-learn-more"
        ></a>
      </p>
    </div>`;
  }

  nameInputTemplate() {
    return html`<input
      type="text"
      id="profile-name"
      size="64"
      aria-errormessage="error-message"
      data-l10n-id="new-profile-page-input-placeholder"
      value=${this.profile.name}
      @input=${super.handleInputEvent}
    />`;
  }

  buttonsTemplate() {
    return html`<moz-button
        data-l10n-id="edit-profile-page-delete-button"
        @click=${this.onDeleteClick}
      ></moz-button>
      <moz-button
        data-l10n-id="new-profile-page-done-button"
        @click=${this.onDoneClick}
        type="primary"
      ></moz-button>`;
  }
}

customElements.define("new-profile-card", NewProfileCard);
