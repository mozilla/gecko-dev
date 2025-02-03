/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import MozInputText from "chrome://global/content/elements/moz-input-text.mjs";

/**
 * A search input custom element.
 *
 * @tagname moz-input-search
 *
 * @property {string} label - The text of the label element
 * @property {string} name - The name of the input control
 * @property {string} value - The value of the input control
 * @property {boolean} disabled - The disabled state of the input control
 * @property {string} iconSrc - The src for an optional icon
 * @property {string} description - The text for the description element that helps describe the input control
 * @property {string} supportPage - Name of the SUMO support page to link to.
 * @property {string} placeholder - Text to display when the input has no value.
 */
export default class MozInputSearch extends MozInputText {
  inputStylesTemplate() {
    return html`${super.inputStylesTemplate()}`;
  }

  inputTemplate() {
    return html`
      <input
        id="input"
        type="search"
        name=${this.name}
        value=${this.value}
        ?disabled=${this.disabled}
        accesskey=${ifDefined(this.accessKey)}
        placeholder=${ifDefined(this.placeholder)}
        aria-describedby="description"
        @input=${this.handleInput}
        @change=${this.redispatchEvent}
      />
    `;
  }
}
customElements.define("moz-input-search", MozInputSearch);
