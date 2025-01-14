/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

const NAVIGATION_TYPE_ICONS = {
  subpage: "chrome://global/skin/icons/arrow-right.svg",
};

/**
 * A button custom element used for navigating between sub-pages and/or opening
 * dialogs or external links.
 *
 * @tagname moz-box-button
 * @property {string} label - Label for the button.
 * @property {string} type - Type of box button, either "subpage" or "external".
 */
export default class MozBoxButton extends MozLitElement {
  static shadowRootOptions = {
    ...MozLitElement.shadowRootOptions,
    delegatesFocus: true,
  };

  static properties = {
    label: { type: String, fluent: true },
    type: { type: String },
  };

  static queries = {
    buttonEl: "button",
    navIcon: ".nav-icon",
  };

  constructor() {
    super();
    this.type = "subpage";
  }

  click() {
    this.buttonEl.click();
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-box-button.css"
      />
      <button>
        ${this.label}
        <img
          class="nav-icon"
          src=${NAVIGATION_TYPE_ICONS[this.type]}
          role="presentation"
        />
      </button>
    `;
  }
}
customElements.define("moz-box-button", MozBoxButton);
