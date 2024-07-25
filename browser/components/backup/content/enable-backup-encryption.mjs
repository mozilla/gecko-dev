/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/backup/password-validation-inputs.mjs";

/**
 * Valid attributes for the enable-backup-encryption dialog type.
 *
 * @see EnableBackupEncryption.type
 */
const VALID_TYPES = Object.freeze({
  SET_PASSWORD: "set-password",
  CHANGE_PASSWORD: "change-password",
});

const VALID_L10N_IDS = new Map([
  [VALID_TYPES.SET_PASSWORD, "enable-backup-encryption-header"],
  [VALID_TYPES.CHANGE_PASSWORD, "change-backup-encryption-header"],
]);

/**
 * The widget for enabling password protection if the backup is not yet
 * encrypted.
 */
export default class EnableBackupEncryption extends MozLitElement {
  static properties = {
    _inputPassValue: { type: String, state: true },
    _passwordsMatch: { type: Boolean, state: true },
    supportBaseLink: { type: String },
    /**
     * The "type" attribute changes the layout.
     *
     * @see VALID_TYPES
     */
    type: { type: String, reflect: true },
  };

  static get queries() {
    return {
      cancelButtonEl: "#backup-enable-encryption-cancel-button",
      confirmButtonEl: "#backup-enable-encryption-confirm-button",
      contentEl: "#backup-enable-encryption-content",
      textHeaderEl: "#backup-enable-encryption-header",
      textDescriptionEl: "#backup-enable-encryption-description",
      passwordInputsEl: "#backup-enable-encryption-password-inputs",
    };
  }

  constructor() {
    super();
    this.supportBaseLink = "";
    this.type = VALID_TYPES.SET_PASSWORD;
    this._inputPassValue = "";
    this._passwordsMatch = false;
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

    this.addEventListener("ValidPasswordsDetected", this);
    this.addEventListener("InvalidPasswordsDetected", this);
  }

  handleEvent(event) {
    if (event.type == "ValidPasswordsDetected") {
      let { password } = event.detail;
      this._passwordsMatch = true;
      this._inputPassValue = password;
    } else if (event.type == "InvalidPasswordsDetected") {
      this._passwordsMatch = false;
      this._inputPassValue = "";
    }
  }

  handleCancel() {
    this.dispatchEvent(
      new CustomEvent("dialogCancel", {
        bubbles: true,
        composed: true,
      })
    );
    this.resetChanges();
  }

  handleConfirm() {
    switch (this.type) {
      case VALID_TYPES.SET_PASSWORD:
        this.dispatchEvent(
          new CustomEvent("enableEncryption", {
            bubbles: true,
            composed: true,
            detail: {
              password: this._inputPassValue,
            },
          })
        );
        break;
      case VALID_TYPES.CHANGE_PASSWORD:
        this.dispatchEvent(
          new CustomEvent("rerunEncryption", {
            bubbles: true,
            composed: true,
            detail: {
              password: this._inputPassValue,
            },
          })
        );
        break;
    }
    this.resetChanges();
  }

  resetChanges() {
    this._inputPassValue = "";
    this._passwordsMatch = false;

    this.passwordInputsEl.dispatchEvent(
      new CustomEvent("resetInputs", { bubbles: true, composed: true })
    );
  }

  descriptionTemplate() {
    return html`
      <div id="backup-enable-encryption-description">
        <span
          id="backup-enable-encryption-description-span"
          data-l10n-id="enable-backup-encryption-description"
        >
          <!--TODO: finalize support page links (bug 1900467)-->
        </span>
        <a
          id="backup-enable-encryption-learn-more-link"
          is="moz-support-link"
          support-page="todo-backup"
          data-l10n-id="enable-backup-encryption-support-link"
        ></a>
      </div>
    `;
  }

  buttonGroupTemplate() {
    return html`
      <moz-button-group id="backup-enable-encryption-button-group">
        <moz-button
          id="backup-enable-encryption-cancel-button"
          @click=${this.handleCancel}
          data-l10n-id="enable-backup-encryption-cancel-button"
        ></moz-button>
        <moz-button
          id="backup-enable-encryption-confirm-button"
          @click=${this.handleConfirm}
          type="primary"
          data-l10n-id="enable-backup-encryption-confirm-button"
          ?disabled=${!this._passwordsMatch}
        ></moz-button>
      </moz-button-group>
    `;
  }

  contentTemplate() {
    return html`
      <div
        id="backup-enable-encryption-wrapper"
        aria-labelledby="backup-enable-encryption-header"
        aria-describedby="backup-enable-encryption-description"
      >
        <h1
          id="backup-enable-encryption-header"
          class="heading-medium"
          data-l10n-id=${ifDefined(VALID_L10N_IDS.get(this.type))}
        ></h1>
        <div id="backup-enable-encryption-content">
          ${this.type === VALID_TYPES.SET_PASSWORD
            ? this.descriptionTemplate()
            : null}
          <password-validation-inputs
            id="backup-enable-encryption-password-inputs"
            .supportBaseLink=${this.supportBaseLink}
          >
          </password-validation-inputs>
        </div>
        ${this.buttonGroupTemplate()}
      </div>
    `;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/backup/enable-backup-encryption.css"
      />
      ${this.contentTemplate()}
    `;
  }
}

customElements.define("enable-backup-encryption", EnableBackupEncryption);
