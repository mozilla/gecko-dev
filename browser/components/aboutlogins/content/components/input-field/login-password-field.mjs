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
    visible: { type: Boolean, reflect: true },
    required: { type: Boolean, reflect: true },
    onRevealClick: { type: Function },
  };

  static queries = {
    input: "input",
    label: "label",
    button: "moz-button",
  };

  constructor() {
    super();
    this.value = "";
  }

  connectedCallback() {
    super.connectedCallback();
    this.addEventListener("input", e => {
      this.value = e.composedTarget.value;
    });
  }

  get #type() {
    return this.visible ? "text" : "password";
  }

  get #password() {
    return !this.newPassword && !this.visible
      ? LoginPasswordField.CONCEALED_PASSWORD_TEXT
      : this.value;
  }

  #revealIconSrc(concealed) {
    return concealed
      ? "chrome://browser/content/aboutlogins/icons/password-hide.svg"
      : "chrome://browser/content/aboutlogins/icons/password.svg";
  }

  updated(changedProperties) {
    if (changedProperties.has("visible") && !changedProperties.visible) {
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
        noteL10nId: "passwords-password-tooltip",
      })}
      <moz-button
        data-l10n-id=${this.visible
          ? "login-item-password-conceal-checkbox"
          : "login-item-password-reveal-checkbox"}
        class="reveal-password-button"
        type="icon ghost"
        iconSrc=${this.#revealIconSrc(this.visible)}
        @mousedown=${() => {
          /* Programmatically focus the button on mousedown instead of waiting for focus on click
           * because the blur event occurs before the click event.
           */
          this.button.focus();
        }}
        @click=${this.onRevealClick}
      ></moz-button>
    `;
  }

  handleFocus() {
    if (this.visible) {
      return;
    }
    this.onRevealClick();
  }

  handleBlur(ev) {
    if (ev.relatedTarget === this.button || ev.relatedTarget === this.label) {
      return;
    }
    if (!this.visible) {
      return;
    }
    this.onRevealClick();
  }
}

customElements.define("login-password-field", LoginPasswordField);
