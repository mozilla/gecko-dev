/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { editableFieldTemplate, stylesTemplate } from "./input-field.mjs";

class LoginUsernameField extends MozLitElement {
  static properties = {
    value: { type: String, reflect: true },
    name: { type: String },
    readonly: { type: Boolean, reflect: true },
    required: { type: Boolean, reflect: true },
  };

  static formAssociated = true;

  static queries = {
    input: "input",
  };

  constructor() {
    super();
    this.value = "";
  }

  connectedCallback() {
    super.connectedCallback();
    this.internals.setFormValue(this.value);
    this.addEventListener("input", e => {
      this.internals.setFormValue(e.composedTarget.value);
    });
  }

  render() {
    return html`
      ${stylesTemplate()}
      ${editableFieldTemplate({
        type: "text",
        value: this.value,
        disabled: this.readonly,
        required: this.required,
        labelL10nId: "login-item-username-label",
        noteL10nId: "passwords-username-tooltip",
      })}
    `;
  }
}

customElements.define("login-username-field", LoginUsernameField);
