/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

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
import "chrome://browser/content/profiles/avatar.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/profiles/profiles-theme-card.mjs";

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
    avatars: { all: "profiles-avatar" },
    headerAvatar: "#header-avatar",
    themeCards: { all: "profiles-theme-card" },
  };

  updateNameDebouncer = null;
  clearSavedMessageTimer = null;

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

    this.setFavicon();
    this.focusInput();
  }

  async getUpdateComplete() {
    const result = await super.getUpdateComplete();

    await Promise.all(
      Array.from(this.themeCards).map(card => card.updateComplete)
    );

    await this.mozCard.updateComplete;

    return result;
  }

  async focusInput() {
    await this.getUpdateComplete();
    this.nameInput.focus();
    this.nameInput.value = "";
    this.nameInput.value = this.profile.name;
  }

  setFavicon() {
    let favicon = document.getElementById("favicon");
    favicon.href = `chrome://browser/content/profiles/assets/16_${this.profile.avatar}.svg`;
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

    this.getUpdateComplete().then(() => {
      for (let t of this.themeCards) {
        t.selected = t.theme.id === newThemeId;
      }
    });

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

    this.profile.avatar = newAvatar;
    RPMSendAsyncMessage("Profiles:UpdateProfileAvatar", this.profile);
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
    return html`<h2 data-l10n-id="edit-profile-page-header"></h2>`;
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
          <span id="error-message"></span>
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

    return this.themes.map(
      t =>
        html`<profiles-theme-card
          @click=${this.handleThemeClick}
          .theme=${t}
          ?selected=${t.isActive}
        ></profiles-theme-card>`
    );
  }

  handleThemeClick(event) {
    this.updateTheme(event.target.theme.id);
  }

  avatarsTemplate() {
    let avatars = ["book", "briefcase", "flower", "heart", "shopping", "star"];

    return avatars.map(
      avatar =>
        html`<profiles-avatar
          @click=${this.handleAvatarClick}
          @keydown=${this.handleAvatarKeyDown}
          value=${avatar}
          ?selected=${avatar === this.profile.avatar}
        ></profiles-avatar>`
    );
  }

  handleAvatarClick(event) {
    for (let a of this.avatars) {
      a.selected = false;
    }

    let selectedAvatar = event.target;
    selectedAvatar.selected = true;

    this.updateAvatar(selectedAvatar.value);
  }

  /*
   * Implements radiogroup arrow key behavior for the avatar picker.
   *
   * The Enter or Space keys are handled by handleAvatarClick.
   */
  handleAvatarKeyDown(event) {
    let currentAvatar = event.target;

    // Wrap around the ends of the list.
    let nextAvatar = currentAvatar.nextElementSibling || this.avatars[0];
    let previousAvatar =
      currentAvatar.previousElementSibling ||
      this.avatars[this.avatars.length - 1];

    // To correctly style the button to match tab focus, we have to focus the
    // button inside each profiles-avatar component's shadow DOM.
    let nextButton = nextAvatar.shadowRoot.querySelector("button");
    let previousButton = previousAvatar.shadowRoot.querySelector("button");

    if (event.code == "ArrowUp" || event.code == "ArrowLeft") {
      event.preventDefault();
      previousButton.focus();
    } else if (event.code == "ArrowDown" || event.code == "ArrowRight") {
      event.preventDefault();
      nextButton.focus();
    }
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
        ><div id="edit-profile-card">
          <img
            id="header-avatar"
            data-l10n-id=${this.profile.avatarL10nId}
            src="chrome://browser/content/profiles/assets/80_${this.profile
              .avatar}.svg"
          />
          <div id="profile-content">
            ${this.headerTemplate()}${this.profilesNameTemplate()}

            <h3 data-l10n-id="edit-profile-page-theme-header"></h3>
            <div id="themes">${this.themesTemplate()}</div>
            <a
              id="more-themes"
              href="https://addons.mozilla.org/firefox/themes/"
              target="_blank"
              @click=${this.onMoreThemesClick}
              data-l10n-id="edit-profile-page-explore-themes"
            ></a>

            <h3 data-l10n-id="edit-profile-page-avatar-header"></h3>
            <div
              id="avatars"
              role="radiogroup"
              aria-labelledby="edit-profile-page-avatar-header"
            >
              ${this.avatarsTemplate()}
            </div>

            <moz-button-group>${this.buttonsTemplate()}</moz-button-group>
          </div>
        </div></moz-card
      >`;
  }
}

customElements.define("edit-profile-card", EditProfileCard);
