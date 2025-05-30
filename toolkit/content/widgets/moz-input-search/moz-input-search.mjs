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
 * @property {string} description - The text for the description element that helps describe the input control
 * @property {string} supportPage - Name of the SUMO support page to link to.
 * @property {string} placeholder - Text to display when the input has no value.
 * @property {string} ariaLabel
 *  The aria-label text for cases where there is no visible label.
 */
export default class MozInputSearch extends MozInputText {
  static properties = {
    ariaLabel: { type: String, mapped: true },
  };

  // The amount of milliseconds that we wait before firing the "search" event.
  static #searchDebounceDelayMs = 500;

  #searchTimer = null;

  #clearSearchTimer() {
    if (this.#searchTimer) {
      clearTimeout(this.#searchTimer);
    }
    this.#searchTimer = null;
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.#clearSearchTimer();
  }

  inputStylesTemplate() {
    return html`${super.inputStylesTemplate()}`;
  }

  handleInput(e) {
    super.handleInput(e);
    this.#clearSearchTimer();
    this.#searchTimer = setTimeout(() => {
      this.dispatchEvent(new CustomEvent("MozInputSearch:search"));
    }, MozInputSearch.#searchDebounceDelayMs);
  }

  inputTemplate() {
    return html`
      <input
        id="input"
        class="with-icon"
        type="search"
        name=${this.name}
        value=${this.value}
        ?disabled=${this.disabled || this.parentDisabled}
        accesskey=${ifDefined(this.accessKey)}
        placeholder=${ifDefined(this.placeholder)}
        aria-label=${ifDefined(this.ariaLabel ?? undefined)}
        aria-describedby="description"
        @input=${this.handleInput}
        @change=${this.redispatchEvent}
      />
    `;
  }
}
customElements.define("moz-input-search", MozInputSearch);
