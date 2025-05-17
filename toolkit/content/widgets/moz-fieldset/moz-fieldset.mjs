/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

/**
 * Fieldset wrapper to lay out form inputs consistently.
 *
 * @tagname moz-fieldset
 * @property {string} label - The label for the fieldset's legend.
 * @property {string} description - The description for the fieldset.
 * @property {string} supportPage - Name of the SUMO support page to link to.
 */
export default class MozFieldset extends MozLitElement {
  static properties = {
    label: { type: String, fluent: true },
    description: { type: String, fluent: true },
    supportPage: { type: String, attribute: "support-page" },
    ariaLabel: { type: String, fluent: true, mapped: true },
    ariaOrientation: { type: String, mapped: true },
  };

  descriptionTemplate() {
    if (this.description) {
      return html`<span id="description" class="description text-deemphasized">
          ${this.description}
        </span>
        ${this.supportPageTemplate()}`;
    }
    return "";
  }

  supportPageTemplate() {
    if (this.supportPage) {
      return html`<a
        is="moz-support-link"
        support-page=${this.supportPage}
        part="support-link"
      ></a>`;
    }
    return html`<slot name="support-link"></slot>`;
  }

  legendTemplate() {
    return html`<legend part="label">${this.label}</legend>`;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-fieldset.css"
      />
      <fieldset
        aria-label=${ifDefined(this.ariaLabel)}
        aria-describedby=${ifDefined(
          this.description ? "description" : undefined
        )}
        aria-orientation=${ifDefined(this.ariaOrientation)}
      >
        ${this.label ? this.legendTemplate() : ""}
        ${!this.description ? this.supportPageTemplate() : ""}
        ${this.descriptionTemplate()}
        <div id="inputs" part="inputs">
          <slot></slot>
        </div>
      </fieldset>
    `;
  }
}
customElements.define("moz-fieldset", MozFieldset);
