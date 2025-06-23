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
 * @property {string} message - THe message of the promo element.
 */
export default class MozPromo extends MozLitElement {
  static properties = {
    type: { type: String, reflect: true },
    heading: { type: String, fluent: true },
    message: { type: String, fluent: true },
  };

  constructor() {
    super();
    this.type = "default";
  }

  headingTemplate() {
    if (this.heading) {
      return html`<strong class="heading heading-medium"
        >${this.heading}</strong
      >`;
    }
    return "";
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-promo.css"
      />
      <div class="container">
        ${this.headingTemplate()}
        <p class="message">${this.message}</p>
      </div>
    `;
  }
}
customElements.define("moz-promo", MozPromo);
