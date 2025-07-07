/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";

const UPDATED_AVATAR_SELECTOR_PREF = "browser.profiles.updated-avatar-selector";

/**
 * Like DeferredTask but usable from content.
 */
class Debounce {
  timeout = null;
  #callback = null;
  #timeoutId = null;

  constructor(callback, timeout) {
    this.#callback = callback;
    this.timeout = timeout;
    this.#timeoutId = null;
  }

  #trigger() {
    this.#timeoutId = null;
    this.#callback();
  }

  arm() {
    this.disarm();
    this.#timeoutId = setTimeout(() => this.#trigger(), this.timeout);
  }

  disarm() {
    if (this.isArmed) {
      clearTimeout(this.#timeoutId);
      this.#timeoutId = null;
    }
  }

  finalize() {
    if (this.isArmed) {
      this.disarm();
      this.#callback();
    }
  }

  get isArmed() {
    return this.#timeoutId !== null;
  }
}

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-card.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button-group.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-visual-picker.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/profiles/avatar.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/profiles/profiles-theme-card.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/profiles/profile-avatar-selector.mjs";

const SAVE_NAME_TIMEOUT = 2000;
const SAVED_MESSAGE_TIMEOUT = 5000;

/**
 * Element used for updating a profile's name, theme, and avatar.
 */
export class EditProfileCard extends MozLitElement {
  static properties = {
    profile: { type: Object },
    profiles: { type: Array },
    themes: { type: Array },
  };

  static queries = {
    mozCard: "moz-card",
    nameInput: "#profile-name",
    errorMessage: "#error-message",
    savedMessage: "#saved-message",
    deleteButton: "#delete-button",
    doneButton: "#done-button",
    moreThemesLink: "#more-themes",
    headerAvatar: "#header-avatar",
    avatarsPicker: "#avatars",
    themesPicker: "#themes",
    avatarSelector: "profile-avatar-selector",
    avatarSelectorLink: "#profile-avatar-selector-link",
  };

  updateNameDebouncer = null;
  clearSavedMessageTimer = null;

  get avatars() {
    return this.avatarsPicker.childElements;
  }

  get themeCards() {
    return this.themesPicker.childElements;
  }

  constructor() {
    super();

    this.updateNameDebouncer = new Debounce(
      () => this.updateName(),
      SAVE_NAME_TIMEOUT
    );

    this.clearSavedMessageTimer = new Debounce(
      () => this.hideSavedMessage(),
      SAVED_MESSAGE_TIMEOUT
    );
  }

  connectedCallback() {
    super.connectedCallback();

    window.addEventListener("beforeunload", this);
    window.addEventListener("pagehide", this);

    if (RPMGetBoolPref(UPDATED_AVATAR_SELECTOR_PREF, false)) {
      document.addEventListener("click", this);
      document.addEventListener("Profiles:CustomAvatarUpload", this);
    }

    this.init().then(() => (this.initialized = true));
  }

  async init() {
    if (this.initialized) {
      return;
    }

    let { currentProfile, profiles, themes, isInAutomation } =
      await RPMSendQuery("Profiles:GetEditProfileContent");

    if (isInAutomation) {
      this.updateNameDebouncer.timeout = 50;
    }

    this.profile = currentProfile;
    this.profiles = profiles;
    this.themes = themes;

    if (this.profile.hasCustomAvatar) {
      this.createAvatarURL();
    }

    this.setFavicon();
  }

  createAvatarURL() {
    if (this.profile.avatarURLs.url16) {
      URL.revokeObjectURL(this.profile.avatarURLs.url16);
      delete this.profile.avatarURLs.url16;
      delete this.profile.avatarURLs.url80;
    }

    if (this.profile.avatarFiles?.file16) {
      const objURL = URL.createObjectURL(this.profile.avatarFiles.file16);
      this.profile.avatarURLs.url16 = objURL;
      this.profile.avatarURLs.url80 = objURL;
    }
  }

  async getUpdateComplete() {
    const result = await super.getUpdateComplete();

    await Promise.all(
      Array.from(this.themeCards).map(card => card.updateComplete)
    );

    await this.mozCard.updateComplete;

    return result;
  }

  setFavicon() {
    let favicon = document.getElementById("favicon");
    favicon.href = this.profile.avatarURLs.url16;
  }

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

  handleEvent(event) {
    switch (event.type) {
      case "beforeunload": {
        let newName = this.nameInput.value.trim();
        if (newName === "") {
          this.showErrorMessage("edit-profile-page-no-name");
          event.preventDefault();
        } else {
          this.updateNameDebouncer.finalize();
        }
        break;
      }
      case "pagehide": {
        RPMSendAsyncMessage("Profiles:PageHide");
        break;
      }
      case "click": {
        if (event.originalTarget.closest("#avatar-selector")) {
          return;
        }
        this.avatarSelector.hidden = true;
        break;
      }
      case "Profiles:CustomAvatarUpload": {
        let { file } = event.detail;
        this.updateAvatar(file);
        break;
      }
    }
  }

  updated() {
    super.updated();

    if (!this.profile) {
      return;
    }

    let { themeFg, themeBg } = this.profile;
    this.headerAvatar.style.fill = themeBg;
    this.headerAvatar.style.stroke = themeFg;
  }

  updateName() {
    this.updateNameDebouncer.disarm();
    this.showSavedMessage();

    let newName = this.nameInput.value.trim();
    if (!newName) {
      return;
    }

    this.profile.name = newName;
    RPMSendAsyncMessage("Profiles:UpdateProfileName", this.profile);
  }

  async updateTheme(newThemeId) {
    if (newThemeId === this.profile.themeId) {
      return;
    }

    let theme = await RPMSendQuery("Profiles:UpdateProfileTheme", newThemeId);
    this.profile.themeId = theme.themeId;
    this.profile.themeFg = theme.themeFg;
    this.profile.themeBg = theme.themeBg;

    this.requestUpdate();
  }

  async updateAvatar(newAvatar) {
    if (newAvatar === this.profile.avatar) {
      return;
    }

    let updatedProfile = await RPMSendQuery("Profiles:UpdateProfileAvatar", {
      avatarOrFile: newAvatar,
    });

    this.profile = updatedProfile;

    if (this.profile.hasCustomAvatar) {
      this.createAvatarURL();
    }

    this.requestUpdate();
    this.setFavicon();
  }

  isDuplicateName(newName) {
    return !!this.profiles.find(
      p => p.id !== this.profile.id && p.name === newName
    );
  }

  async handleInputEvent() {
    this.hideSavedMessage();
    let newName = this.nameInput.value.trim();
    if (newName === "") {
      this.showErrorMessage("edit-profile-page-no-name");
    } else if (this.isDuplicateName(newName)) {
      this.showErrorMessage("edit-profile-page-duplicate-name");
    } else {
      this.hideErrorMessage();
      this.updateNameDebouncer.arm();
    }
  }

  showErrorMessage(l10nId) {
    this.updateNameDebouncer.disarm();
    document.l10n.setAttributes(this.errorMessage, l10nId);
    this.errorMessage.parentElement.hidden = false;
    this.nameInput.setCustomValidity("invalid");
  }

  hideErrorMessage() {
    this.errorMessage.parentElement.hidden = true;
    this.nameInput.setCustomValidity("");
  }

  showSavedMessage() {
    this.savedMessage.parentElement.hidden = false;
    this.clearSavedMessageTimer.arm();
  }

  hideSavedMessage() {
    this.savedMessage.parentElement.hidden = true;
    this.clearSavedMessageTimer.disarm();
  }

  headerTemplate() {
    return html`<h1
      id="profile-header"
      data-l10n-id="edit-profile-page-header"
    ></h1>`;
  }

  nameInputTemplate() {
    return html`<input
      type="text"
      id="profile-name"
      size="64"
      aria-errormessage="error-message"
      value=${this.profile.name}
      @input=${this.handleInputEvent}
    />`;
  }

  profilesNameTemplate() {
    return html`<div id="profile-name-area">
      <label
        data-l10n-id="edit-profile-page-profile-name-label"
        for="profile-name"
      ></label>
      ${this.nameInputTemplate()}
      <div class="message-parent">
        <span class="message" hidden
          ><img
            class="message-icon"
            id="error-icon"
            src="chrome://global/skin/icons/info.svg"
          />
          <span id="error-message" role="alert"></span>
        </span>
        <span class="message" hidden
          ><img
            class="message-icon"
            id="saved-icon"
            src="chrome://global/skin/icons/check-filled.svg"
          />
          <span
            id="saved-message"
            data-l10n-id="edit-profile-page-profile-saved"
          ></span>
        </span>
      </div>
    </div>`;
  }

  themesTemplate() {
    if (!this.themes) {
      return null;
    }

    return html`<moz-visual-picker
      type="listbox"
      id="themes"
      value=${this.profile.themeId}
      data-l10n-id="edit-profile-page-theme-header-2"
      name="theme"
      @change=${this.handleThemeChange}
    >
      ${this.themes.map(
        t =>
          html`<moz-visual-picker-item
            class="theme-item"
            l10nId=${ifDefined(t.dataL10nId)}
            name=${ifDefined(t.name)}
            value=${t.id}
          >
            <profiles-theme-card
              aria-hidden="true"
              .theme=${t}
              value=${t.id}
            ></profiles-theme-card>
          </moz-visual-picker-item>`
      )}
    </moz-visual-picker>`;
  }

  handleThemeChange() {
    this.updateTheme(this.themesPicker.value);
  }

  avatarsTemplate() {
    if (RPMGetBoolPref(UPDATED_AVATAR_SELECTOR_PREF, false)) {
      return null;
    }

    let avatars = ["book", "briefcase", "flower", "heart", "shopping", "star"];

    return html`<moz-visual-picker
      type="listbox"
      value=${this.profile.avatar}
      data-l10n-id="edit-profile-page-avatar-header-2"
      name="avatar"
      id="avatars"
      @change=${this.handleAvatarChange}
      >${avatars.map(
        avatar =>
          html`<moz-visual-picker-item
            class="avatar-item"
            l10nId=${this.getAvatarL10nId(avatar)}
            value=${avatar}
            ><profiles-avatar value=${avatar}></profiles-avatar
          ></moz-visual-picker-item>`
      )}</moz-visual-picker
    >`;
  }

  headerAvatarTemplate() {
    if (RPMGetBoolPref(UPDATED_AVATAR_SELECTOR_PREF, false)) {
      return html`<div class="avatar-header-content">
        <img
          id="header-avatar"
          data-l10n-id=${this.profile.avatarL10nId}
          src=${this.profile.avatarURLs.url80}
        />
        <a
          id="profile-avatar-selector-link"
          @click=${this.toggleAvatarSelectorCard}
          data-l10n-id="edit-profile-page-avatar-selector-opener-link"
        ></a>
        <div class="avatar-selector-parent">
          <profile-avatar-selector
            hidden
            value=${this.profile.avatar}
          ></profile-avatar-selector>
        </div>
      </div>`;
    }

    return html`<img
      id="header-avatar"
      data-l10n-id=${this.profile.avatarL10nId}
      src=${this.profile.avatarURLs.url80}
    />`;
  }

  toggleAvatarSelectorCard(event) {
    event.stopPropagation();
    this.avatarSelector.hidden = !this.avatarSelector.hidden;
  }

  handleAvatarChange() {
    this.updateAvatar(this.avatarsPicker.value);
  }

  onDeleteClick() {
    window.removeEventListener("beforeunload", this);
    RPMSendAsyncMessage("Profiles:OpenDeletePage");
  }

  onDoneClick() {
    let newName = this.nameInput.value.trim();
    if (newName === "") {
      this.showErrorMessage("edit-profile-page-no-name");
    } else if (this.isDuplicateName(newName)) {
      this.showErrorMessage("edit-profile-page-duplicate-name");
    } else {
      this.updateNameDebouncer.finalize();
      // Remove the pagehide listener early to prevent double-counting the
      // profiles.existing.closed Glean event.
      window.removeEventListener("pagehide", this);
      RPMSendAsyncMessage("Profiles:CloseProfileTab");
    }
  }

  onMoreThemesClick() {
    // Include the starting URI because the page will navigate before the
    // event is asynchronously handled by Glean code in the parent actor.
    RPMSendAsyncMessage("Profiles:MoreThemes", {
      source: window.location.href,
    });
  }

  buttonsTemplate() {
    return html`<moz-button
        id="delete-button"
        data-l10n-id="edit-profile-page-delete-button"
        @click=${this.onDeleteClick}
      ></moz-button>
      <moz-button
        id="done-button"
        data-l10n-id="new-profile-page-done-button"
        @click=${this.onDoneClick}
        type="primary"
      ></moz-button>`;
  }

  render() {
    if (!this.profile) {
      return null;
    }

    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/profiles/edit-profile-card.css"
      />
      <link
        rel="stylesheet"
        href="chrome://global/skin/in-content/common.css"
      />
      <moz-card
        ><div id="edit-profile-card" aria-labelledby="profile-header">
          ${this.headerAvatarTemplate()}
          <div id="profile-content">
            ${this.headerTemplate()}${this.profilesNameTemplate()}
            ${this.themesTemplate()}

            <a
              id="more-themes"
              href="https://addons.mozilla.org/firefox/themes/"
              target="_blank"
              @click=${this.onMoreThemesClick}
              data-l10n-id="edit-profile-page-explore-themes"
            ></a>

            ${this.avatarsTemplate()}

            <moz-button-group>${this.buttonsTemplate()}</moz-button-group>
          </div>
        </div></moz-card
      >`;
  }
}

customElements.define("edit-profile-card", EditProfileCard);
