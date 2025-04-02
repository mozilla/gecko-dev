/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

/**
 * An element used to group combinations of moz-box-item, moz-box-link, and
 * moz-box-button elements and provide the expected styles.
 *
 * @tagname moz-box-group
 * * @slot default - Slot for rendering various moz-box-* elements.
 */
export default class MozBoxGroup extends MozLitElement {
  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-box-group.css"
      />
      <slot></slot>
    `;
  }
}
customElements.define("moz-box-group", MozBoxGroup);
