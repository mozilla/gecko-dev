/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import { MozBaseInputElement } from "../lit-utils.mjs";

/**
 * A text input custom element.
 *
 * @tagname moz-input-text
 * @property {string} label - The text of the label element
 * @property {string} name - The name of the input control
 * @property {string} value - The value of the input control
 * @property {boolean} disabled - The disabled state of the input control
 * @property {boolean} readonly - The readonly state of the input control
 * @property {string} iconSrc - The src for an optional icon
 * @property {string} description - The text for the description element that helps describe the input control
 * @property {string} supportPage - Name of the SUMO support page to link to.
 * @property {string} placeholder - Text to display when the input has no value.
 */
export default class MozInputText extends MozBaseInputElement {
  static properties = {
    placeholder: { type: String, fluent: true },
    readonly: { type: Boolean, mapped: true },
  };
  static inputLayout = "block";

  constructor() {
    super();
    this.value = "";
    this.readonly = false;
  }

  inputStylesTemplate() {
    return html`<link
      rel="stylesheet"
      href="chrome://global/content/elements/moz-input-text.css"
    />`;
  }

  handleInput(e) {
    this.value = e.target.value;
  }

  inputTemplate(options = {}) {
    let { type = "text", classes, styles, inputValue } = options;

    return html`
      <input
        id="input"
        type=${type}
        class=${ifDefined(classes)}
        style=${ifDefined(styles)}
        name=${this.name}
        value=${inputValue || this.value}
        ?disabled=${this.disabled}
        ?readonly=${this.readonly}
        accesskey=${ifDefined(this.accessKey)}
        placeholder=${ifDefined(this.placeholder)}
        aria-describedby="description"
        @input=${this.handleInput}
        @change=${this.redispatchEvent}
      />
    `;
  }
}
customElements.define("moz-input-text", MozInputText);
