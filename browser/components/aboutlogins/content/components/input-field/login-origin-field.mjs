/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { editableFieldTemplate, stylesTemplate } from "./input-field.mjs";

class LoginOriginField extends MozLitElement {
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

  addHTTPSPrefix(e) {
    const input = e.composedTarget;
    let originValue = input.value.trim();
    if (!originValue) {
      return;
    }

    if (!originValue.match(/:\/\//)) {
      input.value = "https://" + originValue;
      input.dispatchEvent(
        new InputEvent("input", {
          composed: true,
          bubbles: true,
        })
      );
    }
  }

  get readonlyTemplate() {
    return html`
      <label
        for="origin"
        class="field-label"
        data-l10n-id="login-item-origin-label"
      >
      </label>
      <a
        id="origin"
        class="origin-input"
        dir="auto"
        target="_blank"
        rel="noreferrer"
        name="origin"
        href=${this.value}
      >
        ${this.value}
      </a>
    `;
  }

  render() {
    return html`
      ${stylesTemplate()}
      ${this.readonly
        ? this.readonlyTemplate
        : editableFieldTemplate({
            type: "url",
            value: this.value,
            required: this.required,
            labelL10nId: "login-item-origin-label",
            noteL10nId: "passwords-origin-tooltip",
            onBlur: e => this.addHTTPSPrefix(e),
          })}
    `;
  }
}

customElements.define("login-origin-field", LoginOriginField);
