/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

/**
 * A promotional callout element.
 *
 * @tagname moz-promo
 * @property {string} type - The type of promo, can be either
 *  "default" or "vibrant". Determines the colors of the promotional
 *  element
 * @property {string} heading - The heading of the promo element.
 * @property {string} message - The message of the promo element.
 * @property {string} imageSrc - The main image of the promo element.
 * @property {string} iconAlignment - How the icon should be aligned. Can be "start", "end", "center".
 */
export default class MozPromo extends MozLitElement {
  static properties = {
    type: { type: String, reflect: true },
    heading: { type: String, fluent: true },
    message: { type: String, fluent: true },
    imageSrc: { type: String, reflect: true },
    imageAlignment: { type: String, reflect: true },
  };

  constructor() {
    super();
    this.type = "default";
    this.imageAlignment = "start";
  }

  updated(changedProperties) {
    // super.updated?.(changedProperties);
    if (changedProperties.has("imageSrc") && this.imageSrc) {
      this.style.setProperty("--promo-image-url", `url("${this.imageSrc}")`);
    }
  }

  headingTemplate() {
    if (this.heading) {
      return html`<strong class="heading heading-medium"
        >${this.heading}</strong
      >`;
    }
    return "";
  }
  imageTemplate() {
    if (this.imageSrc) {
      return html` <div class="image-container"></div> `;
    }
    return "";
  }
  render() {
    let imageStartAligned = this.imageAlignment == "start";
    return html` <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-promo.css"
      />
      <div class="container">
        ${imageStartAligned ? this.imageTemplate() : ""}
        <div class="text-container">
          ${this.headingTemplate()}
          <p class="message">${this.message}</p>
        </div>
        ${!imageStartAligned ? this.imageTemplate() : ""}
      </div>`;
  }
}
customElements.define("moz-promo", MozPromo);
