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

export class LoginForm extends MozLitElement {
  static properties = {
    type: { type: String, reflect: true },
  };

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
        <form>
          <moz-fieldset data-l10n-id=${heading}>
            <login-origin-field></login-origin-field>
            <login-username-field></login-username-field>
            <login-password-field></login-password-field>
            <moz-button-group>
              <moz-button data-l10n-id="login-item-cancel-button"></moz-button>
              <moz-button
                data-l10n-id="login-item-save-new-button"
                type="primary"
              >
              </moz-button>
            </moz-button-group>
          </moz-fieldset>
        </form>
      </moz-card>`;
  }
}

customElements.define("login-form", LoginForm);
