/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

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

  static queries = { checkbox: "moz-checkbox" };

  constructor() {
    super();

    this.init();
  }

  async init() {
    await SelectableProfileService.init();
    this.profiles = await SelectableProfileService.getAllProfiles();

    if (!this.profiles.length) {
      SelectableProfileService.showProfileSelectorWindow(false);
    }
  }

  handleCheckboxToggle() {
    SelectableProfileService.showProfileSelectorWindow(this.checkbox.checked);
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
        ${this.profiles.map(p => html`<h3>${p.id} - ${p.name}</h3>`)}
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
