/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/backup/password-rules-tooltip.mjs";

/**
 * The widget for enabling password protection if the backup is not yet
 * encrypted.
 */
export default class PasswordValidationInputs extends MozLitElement {
  static properties = {
    _hasCommon: { type: Boolean, state: true },
    _hasEmail: { type: Boolean, state: true },
    _passwordsMatch: { type: Boolean, state: true },
    _passwordsValid: { type: Boolean, state: true },
    _showRules: { type: Boolean, state: true },
    _tooShort: { type: Boolean, state: true },
    /**
     * If, by chance, there is focus on a focusable element in the tooltip,
     * track the focus state so that we can keep the tooltip open.
     */
    _tooltipFocus: { type: Boolean, state: true },
    supportBaseLink: { type: String },
  };

  static get queries() {
    return {
      formEl: "#password-inputs-form",
      inputNewPasswordEl: "#new-password-input",
      inputRepeatPasswordEl: "#repeat-password-input",
      passwordRulesEl: "#password-rules",
    };
  }

  constructor() {
    super();
    this.supportBaseLink = "";
    this._tooShort = true;
    this._hasCommon = false;
    this._hasEmail = false;
    this._passwordsMatch = false;
    this._passwordsValid = false;
    this._tooltipFocus = false;
  }

  connectedCallback() {
    super.connectedCallback();
    this.addEventListener("resetInputs", this.handleReset);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.removeEventListener("resetInputs", this.handleReset);
  }

  handleReset() {
    this.formEl.reset();
    this._showRules = false;
    this._hasCommon = false;
    this._hasEmail = false;
    this._tooShort = true;
    this._passwordsMatch = false;
    this._passwordsValid = false;
    this._tooltipFocus = false;
  }

  handleFocusNewPassword() {
    this._showRules = true;
  }

  handleBlurNewPassword(event) {
    this._showRules = !event.target.checkValidity();
  }

  handleChangeNewPassword() {
    this.updatePasswordValidity();
  }

  handleChangeRepeatPassword() {
    this.updatePasswordValidity();
  }

  updatePasswordValidity() {
    const emailRegex = /^[\w!#$%&'*+/=?^`{|}~.-]+@[A-Z0-9-]+\.[A-Z0-9.-]+$/i;
    this._hasEmail = emailRegex.test(this.inputNewPasswordEl.value);
    if (this._hasEmail) {
      // TODO: we need a localized string for this error (bug 1909983)
      this.inputNewPasswordEl.setCustomValidity("TODO: no emails");
    } else {
      this.inputNewPasswordEl.setCustomValidity("");
    }

    const newPassValidity = this.inputNewPasswordEl.validity;
    this._tooShort = newPassValidity?.valueMissing || newPassValidity?.tooShort;

    this._passwordsMatch =
      this.inputNewPasswordEl.value == this.inputRepeatPasswordEl.value;
    if (!this._passwordsMatch) {
      // TODO: we need a localized string for this error  (bug 1909983)
      this.inputRepeatPasswordEl.setCustomValidity("TODO: not matching");
    } else {
      this.inputRepeatPasswordEl.setCustomValidity("");
    }

    const repeatPassValidity = this.inputRepeatPasswordEl.validity;
    this._passwordsValid =
      newPassValidity?.valid &&
      repeatPassValidity?.valid &&
      this._passwordsMatch;

    /**
     * This step may involve async validation with BackupService. For instance, we have to
     * check against a list of common passwords (bug 1905140) and display an error message if an
     * issue occurs (bug 1905145).
     */
  }

  handleTooltipFocus() {
    this._tooltipFocus = true;
  }

  handleTooltipBlur() {
    this._tooltipFocus = false;
  }

  /**
   * Dispatches a custom event whenever validity changes.
   *
   * @param {Map<string, any>} changedProperties a Map of recently changed properties and their new values
   */
  updated(changedProperties) {
    if (!changedProperties.has("_passwordsValid")) {
      return;
    }

    if (this._passwordsValid) {
      this.dispatchEvent(
        new CustomEvent("ValidPasswordsDetected", {
          bubbles: true,
          composed: true,
          detail: {
            password: this.inputNewPasswordEl.value,
          },
        })
      );
    } else {
      this.dispatchEvent(
        new CustomEvent("InvalidPasswordsDetected", {
          bubbles: true,
          composed: true,
        })
      );
    }
  }

  contentTemplate() {
    return html`
      <div id="password-inputs-wrapper" aria-live="polite">
        <form id="password-inputs-form">
          <!--TODO: (bug 1909983) change first input field label for the "change-password" dialog-->
          <label id="new-password-label" for="new-password-input">
            <div id="new-password-label-wrapper-span-input">
              <span
                id="new-password-span"
                data-l10n-id="enable-backup-encryption-create-password-label"
              ></span>
              <input
                type="password"
                id="new-password-input"
                minlength="8"
                required
                @input=${this.handleChangeNewPassword}
                @focus=${this.handleFocusNewPassword}
                @blur=${this.handleBlurNewPassword}
              />
              <!--TODO: (bug 1909984) improve how we read out the first input field for screen readers-->
            </div>
          </label>
          <!--TODO: (bug 1909984) look into how the tooltip vs dialog behaves when pressing the ESC key-->
          <password-rules-tooltip
            id="password-rules"
            class=${!this._showRules && !this._tooltipFocus ? "hidden" : ""}
            .hasCommon=${this._hasCommon}
            .hasEmail=${this._hasEmail}
            .tooShort=${this._tooShort}
            .supportBaseLink=${this.supportBaseLink}
            @focus=${this.handleTooltipFocus}
            @blur=${this.handleTooltipBlur}
          ></password-rules-tooltip>
          <label id="repeat-password-label" for="repeat-password-input">
            <span
              id="repeat-password-span"
              data-l10n-id="enable-backup-encryption-repeat-password-label"
            ></span>
            <input
              type="password"
              id="repeat-password-input"
              minlength="8"
              required
              @input=${this.handleChangeRepeatPassword}
            />
          </label>
        </form>
      </div>
    `;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/backup/password-validation-inputs.css"
      />
      ${this.contentTemplate()}
    `;
  }
}

customElements.define("password-validation-inputs", PasswordValidationInputs);
