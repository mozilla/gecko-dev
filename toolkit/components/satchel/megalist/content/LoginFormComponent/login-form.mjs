/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, when } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/* eslint-disable-next-line import/no-unassigned-import, mozilla/no-browser-refs-in-toolkit */
import "chrome://browser/content/aboutlogins/components/input-field/login-origin-field.mjs";
/* eslint-disable-next-line import/no-unassigned-import, mozilla/no-browser-refs-in-toolkit */
import "chrome://browser/content/aboutlogins/components/input-field/login-username-field.mjs";
/* eslint-disable-next-line import/no-unassigned-import, mozilla/no-browser-refs-in-toolkit */
import "chrome://browser/content/aboutlogins/components/input-field/login-password-field.mjs";

/* eslint-disable-next-line import/no-unassigned-import, mozilla/no-browser-refs-in-toolkit */
import "chrome://browser/content/aboutlogins/components/login-message-popup.mjs";

export class LoginForm extends MozLitElement {
  static properties = {
    type: { type: String, reflect: true },
    onSaveClick: { type: Function },
    onDeleteClick: { type: Function },
    onClose: { type: Function },
    originValue: { type: String },
    usernameValue: { type: String },
    passwordValue: { type: String },
    passwordVisible: { type: Boolean },
    onPasswordRevealClick: { type: Function },
    _showDeleteCard: { type: Boolean, state: true },
  };

  static queries = {
    formEl: "form",
    originField: "login-origin-field",
    usernameField: "login-username-field",
    passwordField: "login-password-field",
    originWarning: "origin-warning",
    passwordWarning: "password-warning",
  };

  constructor() {
    super();
    this.originValue = "";
    this.usernameValue = "";
    this.passwordValue = "";
    this._showDeleteCard = false;
    this.onPasswordRevealClick = () => {
      this.passwordVisible = !this.passwordVisible;
    };
  }

  #removeWarning(warning) {
    if (warning.classList.contains("invalid-input")) {
      warning.classList.remove("invalid-input");
    }
  }

  #shouldShowWarning(input, warning) {
    if (!input.checkValidity()) {
      warning.setAttribute("message", input.validationMessage);
      warning.classList.add("invalid-input");
      return true;
    }
    this.#removeWarning(warning);
    return false;
  }

  onInput(e) {
    const field = e.target;
    const warning =
      field.name === "origin" ? this.originWarning : this.passwordWarning;

    if (field.input.checkValidity()) {
      this.#removeWarning(warning);
    }
  }

  onSubmit(e) {
    e.preventDefault();

    if (
      this.type !== "edit" &&
      this.#shouldShowWarning(this.originField.input, this.originWarning)
    ) {
      return;
    }

    if (
      this.#shouldShowWarning(this.passwordField.input, this.passwordWarning)
    ) {
      return;
    }

    const loginForm = {
      origin: this.originValue || this.originField.input.value,
      username: this.usernameField.input.value,
      password: this.passwordField.value,
    };
    this.onSaveClick(loginForm);
  }

  #toggleDeleteCard() {
    this._showDeleteCard = !this._showDeleteCard;
  }

  #renderDeleteCard() {
    return html` <link
        rel="stylesheet"
        href="chrome://global/content/megalist/LoginFormComponent/login-form.css"
      />
      <moz-card class="remove-login-card">
        <div class="remove-card-back">
          <moz-button
            type="icon ghost"
            iconSrc="chrome://browser/skin/back.svg"
            @click=${this.#toggleDeleteCard}
          >
          </moz-button>
          <p data-l10n-id="passwords-remove-login-card-back-message"></p>
        </div>
        <div class="remove-card-text">
          <h3 data-l10n-id="passwords-remove-login-card-title"></h3>
          <p data-l10n-id="passwords-remove-login-card-message"></p>
        </div>
        <moz-button-group>
          <moz-button
            data-l10n-id="passwords-remove-login-card-cancel-button"
            @click=${this.#toggleDeleteCard}
          >
          </moz-button>
          <moz-button
            type="destructive"
            data-l10n-id="passwords-remove-login-card-remove-button"
            @click=${this.onDeleteClick}
          >
          </moz-button>
        </moz-button-group>
      </moz-card>`;
  }

  render() {
    if (this._showDeleteCard) {
      return this.#renderDeleteCard();
    }

    const heading =
      this.type !== "edit" ? "passwords-create-label" : "passwords-edit-label";

    return html`<link
        rel="stylesheet"
        href="chrome://global/content/megalist/LoginFormComponent/login-form.css"
      />
      <moz-card>
        ${when(
          this.type === "edit",
          () => html`
            <div class="delete-login-button-container">
              <moz-button
                class="delete-login-button"
                data-l10n-id="passwords-remove-label"
                type="icon"
                iconSrc="chrome://global/skin/icons/delete.svg"
                @click=${this.#toggleDeleteCard}
              ></moz-button>
            </div>
          `
        )}

        <form
          role="region"
          aria-label=${heading}
          @submit=${e => this.onSubmit(e)}
        >
          <moz-fieldset data-l10n-id=${heading}>
            <div class="field-container">
              <login-origin-field
                name="origin"
                required
                ?readonly=${this.type === "edit"}
                value=${this.originValue}
                @input=${e => this.onInput(e)}
              >
              </login-origin-field>
              <origin-warning arrowdirection="down"></origin-warning>
            </div>
            <login-username-field
              name="username"
              value=${this.usernameValue}
            ></login-username-field>
            <div class="field-container">
              <login-password-field
                name="password"
                required
                ?visible=${this.passwordVisible}
                ?newPassword=${this.type !== "edit"}
                .value=${this.passwordValue}
                .onRevealClick=${this.onPasswordRevealClick}
                @input=${e => this.onInput(e)}
              ></login-password-field>
              <password-warning
                isNewLogin
                arrowdirection="down"
              ></password-warning>
            </div>
            <moz-button-group>
              <moz-button
                data-l10n-id="login-item-cancel-button"
                @click=${this.onClose}
              ></moz-button>
              <moz-button
                data-l10n-id="login-item-save-new-button"
                type="primary"
                @click=${() => this.formEl.requestSubmit()}
              >
              </moz-button>
            </moz-button-group>
          </moz-fieldset>
        </form>
      </moz-card>`;
  }
}

customElements.define("login-form", LoginForm);
