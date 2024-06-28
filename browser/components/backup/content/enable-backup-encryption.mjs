/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

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
    passwordsMatch: { type: Boolean, reflect: true },
    passwordsRequired: { type: Boolean, reflect: true },
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
      inputNewPasswordEl: "#new-password-input",
      inputRepeatPasswordEl: "#repeat-password-input",
      textDescriptionEl: "#backup-enable-encryption-description",
    };
  }

  constructor() {
    super();
    this.passwordsMatch = false;
    this.passwordsRequired = true;
    this.type = VALID_TYPES.SET_PASSWORD;
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
    this.resetChanges();
  }

  handleConfirm() {
    if (this.type == VALID_TYPES.SET_PASSWORD) {
      this.dispatchEvent(
        new CustomEvent("enableEncryption", {
          bubbles: true,
          composed: true,
          detail: {
            password: this.inputNewPasswordEl.value,
          },
        })
      );
    } else {
      // TODO: dispatch event to update existing recovery code
    }
    this.resetChanges();
  }

  handleChangeNewPassword() {
    this.updatePasswordValidity();
  }

  handleChangeRepeatPassword() {
    this.updatePasswordValidity();
  }

  updatePasswordValidity() {
    // If the "required" attribute was previously removed, add it back
    // to make password validation work as expected.
    if (!this.passwordsRequired) {
      this.passwordsRequired = true;
    }

    let isNewPasswordInputValid = this.inputNewPasswordEl?.checkValidity();
    let isRepeatPasswordInputValid =
      this.inputRepeatPasswordEl?.checkValidity();
    /**
     * TODO: Before confirmation, verify FxA format rules (bug 1896772).
     * This step may involve async validation with BackupService. For instance, we have to
     * check against a list of common passwords (bug 1905140) and display a message if an
     * issue occurs (bug 1905145).
     */
    this.passwordsMatch =
      isNewPasswordInputValid &&
      isRepeatPasswordInputValid &&
      this.inputNewPasswordEl.value == this.inputRepeatPasswordEl.value;
  }

  resetChanges() {
    this.passwordsMatch = false;
    this.inputNewPasswordEl.value = "";
    this.inputRepeatPasswordEl.value = "";
    // Temporarily remove "required" attribute to remove styles for invalid inputs.
    // The attribute will be added again when we run validation.
    this.passwordsRequired = false;
  }

  contentTemplate() {
    return html`
      <form
        id="backup-enable-encryption-wrapper"
        aria-labelledby="backup-enable-encryption-header"
        aria-describedby="backup-enable-encryption-description"
      >
        <h1
          id="backup-enable-encryption-header"
          class="heading-medium"
          data-l10n-id=${ifDefined(VALID_L10N_IDS.get(this.type))}
        ></h1>
        <main id="backup-enable-encryption-content">
          ${this.type === VALID_TYPES.SET_PASSWORD
            ? html` <div id="backup-enable-encryption-description">
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
              </div>`
            : null}

          <fieldset id="passwords">
            <label id="new-password-label" for="new-password-input">
              <span
                id="new-password-span"
                data-l10n-id="enable-backup-encryption-create-password-label"
              ></span>
              <input
                type="password"
                id="new-password-input"
                ?required=${this.passwordsRequired}
                @input=${this.handleChangeNewPassword}
              />
            </label>
            <label id="repeat-password-label" for="repeat-password-input">
              <span
                id="repeat-password-span"
                data-l10n-id="enable-backup-encryption-repeat-password-label"
              ></span>
              <input
                type="password"
                id="repeat-password-input"
                ?required=${this.passwordsRequired}
                @input=${this.handleChangeRepeatPassword}
              />
            </label>
          </fieldset>
        </main>

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
            ?disabled=${!this.passwordsMatch}
          ></moz-button>
        </moz-button-group>
      </form>
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
