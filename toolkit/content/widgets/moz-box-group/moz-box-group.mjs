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
 * @property {string} type - The type of the group, either "list" or undefined.
 * @slot default - Slot for rendering various moz-box-* elements.
 * @slot <index> - Slots used to assign moz-box-* elements to <li> elements when
 *   the group is type="list".
 */
export default class MozBoxGroup extends MozLitElement {
  static properties = {
    type: { type: String },
    listItems: { type: Array, state: true },
  };

  constructor() {
    super();
    this.listItems = [];
  }

  slotTemplate() {
    if (this.type == "list") {
      return html`<ul class="list">
          ${this.listItems.map((_, i) => {
            return html`<li>
              <slot name=${i}></slot>
            </li> `;
          })}
        </ul>
        <slot hidden @slotchange=${this.handleSlotchange}></slot>`;
    }
    return html`<slot></slot>`;
  }

  handleSlotchange() {
    let boxElements = this.querySelectorAll(
      "moz-box-item, moz-box-button, moz-box-link"
    );
    this.listItems = Array.from(boxElements);
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-box-group.css"
      />
      ${this.slotTemplate()}
    `;
  }

  updated(changedProperties) {
    if (changedProperties.has("listItems") && this.listItems.length) {
      this.listItems.forEach((item, i) => {
        item.slot = i;
      });
    }
  }
}
customElements.define("moz-box-group", MozBoxGroup);
