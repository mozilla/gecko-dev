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
      JSON.stringify({ profilename: this.data.name })
    );

    this.initialized = true;
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
          <img width="80" height="80" src="${this.data.avatar}" />
          <div id="profile-content">
            <h1
              data-l10n-id="delete-profile-header"
              data-l10n-args="${JSON.stringify({
                profilename: this.data.name,
              })}"
            ></h1>
            <h2 data-l10n-id="delete-profile-description"></h2>
            <ul>
              <li id="windows">
                <span data-l10n-id="delete-profile-windows"></span>
                <span class="count">${this.data.windowCount}</span>
              </li>
              <li id="tabs">
                <span data-l10n-id="delete-profile-tabs"></span>
                <span class="count">${this.data.tabCount}</span>
              </li>
              <li id="bookmarks">
                <span data-l10n-id="delete-profile-bookmarks"></span>
                <span class="count">${this.data.bookmarkCount}</span>
              </li>
              <li id="history">
                <span data-l10n-id="delete-profile-history"></span>
                <span class="count">${this.data.historyCount}</span>
              </li>
              <li id="autofill">
                <span data-l10n-id="delete-profile-autofill"></span>
                <span class="count">${this.data.autofillCount}</span>
              </li>
              <li id="logins">
                <span data-l10n-id="delete-profile-logins"></span>
                <span class="count">${this.data.loginCount}</span>
              </li>
            </ul>
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
