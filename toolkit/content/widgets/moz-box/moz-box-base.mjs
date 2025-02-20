/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { classMap, html } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

/**
 * Base class for moz-box-* elements providing common properties and templates.
 *
 * @property {string} label - The text for the label element.
 * @property {string} description - The text for the description element.
 * @property {string} iconSrc - The src for an optional icon.
 */
export default class MozBoxBase extends MozLitElement {
  static properties = {
    label: { type: String, fluent: true },
    description: { type: String, fluent: true },
    iconSrc: { type: String },
  };

  constructor() {
    super();
    this.label = "";
    this.description = "";
    this.iconSrc = "";
  }

  get labelEl() {
    return this.renderRoot.querySelector(".label");
  }

  get descriptionEl() {
    return this.renderRoot.querySelector(".description");
  }

  get iconEl() {
    return this.renderRoot.querySelector(".icon");
  }

  stylesTemplate() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-box-common.css"
      />
      <link
        rel="stylesheet"
        href="chrome://global/skin/design-system/text-and-typography.css"
      />
    `;
  }

  textTemplate() {
    return html`<div
      class=${classMap({
        "text-content": true,
        "has-icon": this.iconSrc,
      })}
    >
      ${this.labelTemplate()}${this.descriptionTemplate()}
    </div>`;
  }

  labelTemplate() {
    if (!this.label) {
      return "";
    }
    return html`<span class="label-wrapper">
      ${this.iconTemplate()}<span class="label">${this.label}</span>
    </span>`;
  }

  iconTemplate() {
    if (!this.iconSrc) {
      return "";
    }
    return html`<img src=${this.iconSrc} role="presentation" class="icon" />`;
  }

  descriptionTemplate() {
    if (!this.description) {
      return "";
    }
    return html`<span class="description text-deemphasized">
      ${this.description}
    </span>`;
  }
}
