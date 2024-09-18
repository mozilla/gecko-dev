/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

/**
 *
 */
export class EditProfileCard extends MozLitElement {
  static properties = {
    profile: { type: Object },
  };

  connectedCallback() {
    super.connectedCallback();

    this.init();
  }

  async init() {
    if (this.initialized) {
      return;
    }

    this.profile = await RPMSendQuery("Profiles:GetEditProfileContent");

    this.initialized = true;
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
      <h1>${this.profile.name}</h1>
      <h3>${JSON.stringify(this.profile)}</h3>`;
  }
}

customElements.define("edit-profile-card", EditProfileCard);
