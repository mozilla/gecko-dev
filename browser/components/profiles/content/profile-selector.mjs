/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/profiles/profile-card.mjs";

const { SelectableProfileService } = ChromeUtils.importESModule(
  "resource:///modules/profiles/SelectableProfileService.sys.mjs"
);

/**
 * The element for display SelectableProfiles in the profile selector window
 */
export class ProfileSelector extends MozLitElement {
  static properties = {
    profiles: { type: Array },
  };

  static queries = {
    checkbox: "moz-checkbox",
    profileCards: { all: "profile-card" },
    createProfileCard: "new-profile-card",
  };

  constructor() {
    super();

    this.init();
  }

  async init() {
    if (this.initialized) {
      return;
    }

    document.addEventListener("LaunchProfile", this);
    document.addEventListener("CreateProfile", this);

    this.selectableProfileService = SelectableProfileService;

    await this.selectableProfileService.init();
    this.profiles = await this.selectableProfileService.getAllProfiles();

    if (!this.profiles.length) {
      this.selectableProfileService.showProfileSelectorWindow(false);
    }

    this.initialized = true;
  }

  handleCheckboxToggle() {
    this.selectableProfileService.showProfileSelectorWindow(
      this.checkbox.checked
    );
  }

  async handleEvent(event) {
    switch (event.type) {
      case "LaunchProfile": {
        let profile = event.detail;
        this.selectableProfileService.launchInstance(profile);
        window.close();
        break;
      }
      case "CreateProfile": {
        await this.selectableProfileService.createNewProfile();
        this.profiles = await this.selectableProfileService.getAllProfiles();
        break;
      }
    }
  }

  render() {
    if (!this.profiles) {
      return null;
    }

    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/profiles/profile-selector.css"
      />
      <link
        rel="stylesheet"
        href="chrome://global/skin/in-content/common.css"
      />
      <img class="logo" src="chrome://branding/content/about-logo.svg" />
      <h1 data-l10n-id="profile-window-heading"></h1>
      <p class="profiles-body-text" data-l10n-id="profile-window-body"></p>
      <div class="profile-list">
        ${this.profiles.map(
          p => html`<profile-card .profile=${p}></profile-card>`
        )}
        <new-profile-card></new-profile-card>
      </div>
      <moz-checkbox
        @click=${this.handleCheckboxToggle}
        data-l10n-id="profile-window-checkbox-label"
        ?checked=${SelectableProfileService.groupToolkitProfile
          .showProfileSelector}
      ></moz-checkbox>`;
  }
}

customElements.define("profile-selector", ProfileSelector);
