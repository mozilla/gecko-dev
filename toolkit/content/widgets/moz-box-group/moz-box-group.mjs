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
  #tabbable = true;

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
      return html`<ul
          class="list"
          aria-orientation="vertical"
          @keydown=${this.handleKeydown}
          @focusin=${this.handleFocus}
          @focusout=${this.handleBlur}
        >
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

  handleKeydown(event) {
    let positionAttr =
      event.target.getAttribute("position") ??
      // handles the case where an interactive element is nested in a moz-box-item
      event.target.closest("moz-box-item").getAttribute("position");
    let currentPosition = parseInt(positionAttr);

    switch (event.key) {
      case "Down":
      case "ArrowDown": {
        let nextItem = this.listItems[currentPosition + 1];
        nextItem?.focus(event);
        break;
      }
      case "Up":
      case "ArrowUp": {
        let prevItem = this.listItems[currentPosition - 1];
        prevItem?.focus(event);
        break;
      }
    }
  }

  handleFocus() {
    if (this.#tabbable) {
      this.#tabbable = false;
      this.listItems.forEach(item => {
        item.setAttribute("tabindex", "-1");
      });
    }
  }

  handleBlur() {
    if (!this.#tabbable) {
      this.#tabbable = true;
      this.listItems.forEach(item => {
        item.removeAttribute("tabindex");
      });
    }
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
        item.setAttribute("position", i);
      });
    }
  }
}
customElements.define("moz-box-group", MozBoxGroup);
