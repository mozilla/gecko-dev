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
    onCancelClick: { type: Function },
  };

  static queries = {
    formEl: "form",
    originField: "login-origin-field",
    passwordField: "login-password-field",
    originWarning: "origin-warning",
    passwordWarning: "password-warning",
  };

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

    if (this.#shouldShowWarning(this.originField.input, this.originWarning)) {
      return;
    }

    if (
      this.#shouldShowWarning(this.passwordField.input, this.passwordWarning)
    ) {
      return;
    }

    this.onSaveClick(new FormData(e.target));
  }

  render() {
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
                type="icon"
                iconSrc="chrome://global/skin/icons/delete.svg"
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
                @input=${e => this.onInput(e)}
              >
              </login-origin-field>
              <origin-warning arrowdirection="down"></origin-warning>
            </div>
            <login-username-field name="username"></login-username-field>
            <div class="field-container">
              <login-password-field
                name="password"
                required
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
                @click=${this.onCancelClick}
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
