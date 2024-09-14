/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { editableFieldTemplate, stylesTemplate } from "./input-field.mjs";

class LoginPasswordField extends MozLitElement {
  static CONCEALED_PASSWORD_TEXT = " ".repeat(8);

  static properties = {
    _value: { type: String, state: true },
    readonly: { type: Boolean, reflect: true },
    visible: { type: Boolean, reflect: true },
  };

  static queries = {
    input: "input",
    button: "button",
  };

  set value(newValue) {
    this._value = newValue;
  }

  get #type() {
    return this.visible ? "text" : "password";
  }

  get #password() {
    return this.readonly && !this.visible
      ? LoginPasswordField.CONCEALED_PASSWORD_TEXT
      : this._value;
  }

  #revealIconSrc(concealed) {
    return concealed
      ? "chrome://browser/content/aboutlogins/icons/password-hide.svg"
      : "chrome://browser/content/aboutlogins/icons/password.svg";
  }

  render() {
    return html`
      ${stylesTemplate()}
      ${editableFieldTemplate({
        type: this.#type,
        value: this.#password,
        labelId: "login-item-password-label",
        disabled: this.readonly,
        onFocus: this.handleFocus,
        onBlur: this.handleBlur,
        labelL10nId: "login-item-password-label",
        noteL10nId: "passwords-password-tooltip",
      })}
      <moz-button
        data-l10n-id=${this.visible
          ? "login-item-password-conceal-checkbox"
          : "login-item-password-reveal-checkbox"}
        class="reveal-password-button"
        type="icon ghost"
        iconSrc=${this.#revealIconSrc(this.visible)}
        @click=${this.toggleVisibility}
      ></moz-button>
    `;
  }

  handleFocus(ev) {
    if (ev.relatedTarget !== this.button) {
      this.visible = true;
    }
  }

  handleBlur(ev) {
    if (ev.relatedTarget !== this.button) {
      this.visible = false;
    }
  }

  toggleVisibility() {
    this.visible = !this.visible;
    if (this.visible) {
      this.onPasswordVisible?.();
    }
  }
}

customElements.define("login-password-field", LoginPasswordField);
