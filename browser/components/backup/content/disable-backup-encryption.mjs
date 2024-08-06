/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * The widget for disabling password protection if the backup is already
 * encrypted.
 */
export default class DisableBackupEncryption extends MozLitElement {
  static get queries() {
    return {
      cancelButtonEl: "#backup-disable-encryption-cancel-button",
      confirmButtonEl: "#backup-disable-encryption-confirm-button",
    };
  }

  /**
   * Dispatches the BackupUI:InitWidget custom event upon being attached to the
   * DOM, which registers with BackupUIChild for BackupService state updates.
   */
  connectedCallback() {
    super.connectedCallback();
    this.dispatchEvent(
      new CustomEvent("BackupUI:InitWidget", { bubbles: true })
    );
  }

  handleCancel() {
    this.dispatchEvent(
      new CustomEvent("dialogCancel", {
        bubbles: true,
        composed: true,
      })
    );
  }

  handleConfirm() {
    this.dispatchEvent(
      new CustomEvent("disableEncryption", {
        bubbles: true,
        composed: true,
      })
    );
  }

  contentTemplate() {
    return html`
      <div
        id="backup-disable-encryption-wrapper"
        aria-labelledby="backup-disable-encryption-header"
        aria-describedby="backup-disable-encryption-description"
      >
        <h1
          id="backup-disable-encryption-header"
          class="heading-medium"
          data-l10n-id="disable-backup-encryption-header"
        ></h1>
        <main id="backup-disable-encryption-content">
          <div id="backup-disable-encryption-description">
            <span
              id="backup-disable-encryption-description-span"
              data-l10n-id="disable-backup-encryption-description"
            >
              <!--TODO: finalize support page links (bug 1900467)-->
            </span>
            <a
              id="backup-disable-encryption-learn-more-link"
              is="moz-support-link"
              support-page="todo-backup"
              data-l10n-id="disable-backup-encryption-support-link"
            ></a>
          </div>
        </main>

        <moz-button-group id="backup-disable-encryption-button-group">
          <moz-button
            id="backup-disable-encryption-cancel-button"
            @click=${this.handleCancel}
            data-l10n-id="disable-backup-encryption-cancel-button"
          ></moz-button>
          <moz-button
            id="backup-disable-encryption-confirm-button"
            @click=${this.handleConfirm}
            type="primary"
            data-l10n-id="disable-backup-encryption-confirm-button"
          ></moz-button>
        </moz-button-group>
      </div>
    `;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/backup/disable-backup-encryption.css"
      />
      ${this.contentTemplate()}
    `;
  }
}

customElements.define("disable-backup-encryption", DisableBackupEncryption);
