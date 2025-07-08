/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

// Functions to wrap a string in a heading.
const HEADING_LEVEL_TEMPLATES = {
  1: label => html`<h1>${label}</h1>`,
  2: label => html`<h2>${label}</h2>`,
  3: label => html`<h3>${label}</h3>`,
  4: label => html`<h4>${label}</h4>`,
  5: label => html`<h5>${label}</h5>`,
  6: label => html`<h6>${label}</h6>`,
};

/**
 * Fieldset wrapper to lay out form inputs consistently.
 *
 * @tagname moz-fieldset
 * @property {string} label - The label for the fieldset's legend.
 * @property {string} description - The description for the fieldset.
 * @property {string} supportPage - Name of the SUMO support page to link to.
 * @property {number} headingLevel - Render the legend in a heading of this level.
 */
export default class MozFieldset extends MozLitElement {
  static properties = {
    label: { type: String, fluent: true },
    description: { type: String, fluent: true },
    supportPage: { type: String, attribute: "support-page" },
    ariaLabel: { type: String, fluent: true, mapped: true },
    ariaOrientation: { type: String, mapped: true },
    headingLevel: { type: Number },
  };

  constructor() {
    super();
    this.headingLevel = -1;
  }

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
    let label =
      HEADING_LEVEL_TEMPLATES[this.headingLevel]?.(this.label) || this.label;
    return html`<legend part="label">${label}</legend>`;
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
