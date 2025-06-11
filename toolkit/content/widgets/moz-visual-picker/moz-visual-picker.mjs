/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import {
  SelectControlItemMixin,
  SelectControlBaseElement,
} from "../lit-select-control.mjs";
import { MozLitElement } from "../lit-utils.mjs";

/**
 * An element that groups related items and allows a user to navigate between
 * them to select an item. The appearance of the items of the group is
 * determined by the consumer.
 *
 * @tagname moz-visual-picker
 * @property {string} label - Label for the group of elements.
 * @property {string} description - Description for the group of elements.
 * @property {string} name
 *  Name used to associate items in the group. Propagates to
 *  moz-visual-picker's children.
 * @property {string} value
 *  Selected value for the group. Changing the value updates the checked
 *  state of moz-visual-picker-item children and vice versa.
 * @slot default - The picker's content, intended for moz-visual-picker-items.
 */
export class MozVisualPicker extends SelectControlBaseElement {
  static childElementName = "moz-visual-picker-item";
  static orientation = "horizontal";
}
customElements.define("moz-visual-picker", MozVisualPicker);

/**
 * Element that allows a user to select one option from a group of options.
 * Visual appearance is determined by the slotted content.
 *
 * @tagname moz-visual-picker-item
 * @property {boolean} checked - Whether or not the item is selected.
 * @property {boolean} disabled - Whether or not the item is disabled.
 * @property {number} itemTabIndex
 *  Tabindex of the input element. Only one item is focusable at a time.
 * @property {string} name
 *  Name of the item, set by the associated moz-visual-picker parent element.
 * @property {string} value - Value of the item.
 * @slot default - The item's content, used for what gets displayed.
 */
export class MozVisualPickerItem extends SelectControlItemMixin(MozLitElement) {
  static queries = {
    itemEl: ".picker-item",
  };

  click() {
    this.itemEl.click();
  }

  focus() {
    this.itemEl.focus();
  }

  blur() {
    this.itemEl.blur();
  }

  handleKeydown(event) {
    if (event.keyCode == KeyEvent.DOM_VK_SPACE) {
      this.handleClick(event);
    }
  }

  handleClick(event) {
    // re-target click events from the slot to the item and handle clicks from
    // space bar keydown.
    event.stopPropagation();
    this.dispatchEvent(
      new Event("click", {
        bubbles: true,
        composed: true,
      })
    );

    super.handleClick();

    // Manually dispatch events since we're not using an input.
    this.dispatchEvent(
      new Event("input", {
        bubbles: true,
        composed: true,
      })
    );
    this.dispatchEvent(
      new Event("change", {
        bubbles: true,
      })
    );
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-visual-picker-item.css"
      />
      <div
        class="picker-item"
        role="radio"
        value=${this.value}
        aria-checked=${this.checked}
        tabindex=${this.itemTabIndex}
        ?checked=${this.checked}
        ?disabled=${this.isDisabled}
        @click=${this.handleClick}
        @keydown=${this.handleKeydown}
      >
        <slot></slot>
      </div>
    `;
  }
}
customElements.define("moz-visual-picker-item", MozVisualPickerItem);
