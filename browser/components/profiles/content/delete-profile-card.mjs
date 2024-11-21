/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-button-group.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-card.mjs";

/**
 * Element used for deleting the currently running profile.
 */
export class DeleteProfileCard extends MozLitElement {
  static properties = {
    data: { type: Object },
  };

  static queries = {
    headerAvatar: "#header-avatar",
  };

  connectedCallback() {
    super.connectedCallback();

    this.init();
  }

  async init() {
    if (this.initialized) {
      return;
    }

    this.data = await RPMSendQuery("Profiles:GetDeleteProfileContent");

    let titleEl = document.querySelector("title");
    titleEl.setAttribute(
      "data-l10n-args",
      JSON.stringify({ profilename: this.data.profile.name })
    );

    this.initialized = true;
  }

  updated() {
    super.updated();

    if (!this.data?.profile) {
      return;
    }

    let { themeFg, themeBg } = this.data.profile;
    this.headerAvatar.style.fill = themeBg;
    this.headerAvatar.style.stroke = themeFg;

    this.setFavicon();
  }

  setFavicon() {
    let favicon = document.getElementById("favicon");
    favicon.href = `chrome://browser/content/profiles/assets/16_${this.data.profile.avatar}.svg`;
  }

  cancelDelete() {
    RPMSendAsyncMessage("Profiles:CancelDelete");
  }

  confirmDelete() {
    RPMSendAsyncMessage("Profiles:DeleteProfile");
  }

  render() {
    if (!this.data) {
      return null;
    }

    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/profiles/delete-profile-card.css"
      />
      <link
        rel="stylesheet"
        href="chrome://global/skin/in-content/common.css"
      />
      <moz-card
        ><div id="delete-profile-card">
          <img
            id="header-avatar"
            width="80"
            height="80"
            src="chrome://browser/content/profiles/assets/80_${this.data.profile
              .avatar}.svg"
          />
          <div id="profile-content">
            <div>
              <h1
                data-l10n-id="delete-profile-header"
                data-l10n-args="${JSON.stringify({
                  profilename: this.data.profile.name,
                })}"
              ></h1>
              <p
                class="sub-header"
                data-l10n-id="delete-profile-description"
              ></p>
            </div>
            <div class="data-list">
              <div class="data-list-item" id="windows">
                <span data-l10n-id="delete-profile-windows"></span>
                <b>${this.data.windowCount}</b>
              </div>
              <div class="data-list-item" id="tabs">
                <span data-l10n-id="delete-profile-tabs"></span>
                <b>${this.data.tabCount}</b>
              </div>
              <div class="data-list-item" id="bookmarks">
                <span data-l10n-id="delete-profile-bookmarks"></span>
                <b>${this.data.bookmarkCount}</b>
              </div>
              <div class="data-list-item" id="history">
                <span data-l10n-id="delete-profile-history"></span>
                <b>${this.data.historyCount}</b>
              </div>
              <div class="data-list-item" id="autofill">
                <span data-l10n-id="delete-profile-autofill"></span>
                <b>${this.data.autofillCount}</b>
              </div>
              <div class="data-list-item" id="logins">
                <span data-l10n-id="delete-profile-logins"></span>
                <b>${this.data.loginCount}</b>
              </div>
            </div>
            <moz-button-group>
              <moz-button
                id="cancel-delete"
                @click=${this.cancelDelete}
                data-l10n-id="delete-profile-cancel"
              ></moz-button>
              <moz-button
                type="destructive"
                id="confirm-delete"
                @click=${this.confirmDelete}
                data-l10n-id="delete-profile-confirm"
              ></moz-button>
            </moz-button-group>
          </div></div
      ></moz-card>`;
  }
}

customElements.define("delete-profile-card", DeleteProfileCard);
