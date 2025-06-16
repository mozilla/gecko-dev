/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { editableFieldTemplate, stylesTemplate } from "./input-field.mjs";

class LoginPasswordField extends MozLitElement {
  static CONCEALED_PASSWORD_TEXT = " ".repeat(8);

  static properties = {
    value: { type: String },
    name: { type: String },
    newPassword: { type: Boolean },
    concealed: { type: Boolean, reflect: true },
    required: { type: Boolean, reflect: true },
  };

  static queries = {
    input: "input",
    label: "label",
    button: "moz-button",
  };

  constructor() {
    super();
    this.value = "";
    this.concealed = true;
  }

  connectedCallback() {
    super.connectedCallback();
    this.addEventListener("input", e => {
      this.value = e.composedTarget.value;
    });
  }

  get #type() {
    return this.concealed ? "password" : "text";
  }

  get #password() {
    return !this.newPassword && this.concealed
      ? LoginPasswordField.CONCEALED_PASSWORD_TEXT
      : this.value;
  }

  updated(changedProperties) {
    if (changedProperties.has("concealed") && !changedProperties.concealed) {
      this.input.selectionStart = this.value.length;
    }
  }

  render() {
    return html`
      ${stylesTemplate()}
      ${editableFieldTemplate({
        type: this.#type,
        value: this.#password,
        labelId: "login-item-password-label",
        disabled: this.readonly,
        required: this.required,
        onFocus: this.handleFocus,
        onBlur: this.handleBlur,
        labelL10nId: "login-item-password-label",
        noteL10nId: "contextual-manager-passwords-password-tooltip",
      })}
    `;
  }

  handleFocus() {
    this.concealed = false;
  }

  handleBlur(ev) {
    if (ev.relatedTarget === this.label) {
      return;
    }
    this.concealed = true;
  }
}

customElements.define("login-password-field", LoginPasswordField);
