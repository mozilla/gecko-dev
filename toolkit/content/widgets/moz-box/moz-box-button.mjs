/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import MozBoxBase from "./moz-box-base.mjs";

/**
 * A button custom element used for navigating between sub-pages or opening
 * dialogs.
 *
 * @tagname moz-box-button
 * @property {string} label - Label for the button.
 * @property {string} description - Descriptive text for the button.
 * @property {string} iconSrc - The src for an optional icon shown next to the label.
 * @property {boolean} disabled - Whether or not the button is disabled.
 */
export default class MozBoxButton extends MozBoxBase {
  static shadowRootOptions = {
    ...super.shadowRootOptions,
    delegatesFocus: true,
  };

  static properties = {
    disabled: { type: Boolean },
  };

  static queries = {
    buttonEl: "button",
    navIcon: ".nav-icon",
  };

  constructor() {
    super();
    this.disabled = false;
  }

  click() {
    this.buttonEl.click();
  }

  render() {
    return html`
      ${super.stylesTemplate()}
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-box-button.css"
      />
      <button ?disabled=${this.disabled}>
        ${super.textTemplate()}
        <img
          class="icon nav-icon"
          src="chrome://global/skin/icons/arrow-right.svg"
          role="presentation"
        />
      </button>
    `;
  }
}
customElements.define("moz-box-button", MozBoxButton);
