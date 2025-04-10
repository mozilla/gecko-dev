/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import { MozBoxBase } from "../lit-utils.mjs";

/**
 * A custom element used for highlighting important information and/or providing
 * context for specific settings.
 *
 * @tagname moz-box-item
 * @property {string} label - Label for the button.
 * @property {string} description - Descriptive text for the button.
 * @property {string} iconSrc - The src for an optional icon shown next to the label.
 * @property {string} layout - Layout style for the box content, either "default" or "large-icon".
 * @slot default - Slot for the box item's content, which overrides label and description.
 * @slot actions - Slot for the actions positioned at the end of the component container.
 * @slot actions-start - Slot for the actions positioned at the start of the component container.
 */
export default class MozBoxItem extends MozBoxBase {
  static properties = {
    layout: { type: String, reflect: true },
  };

  static queries = {
    defaultSlotEl: "slot:not([name])",
    actionsStartSlotEl: "slot[name=actions-start]",
    actionsSlotEl: "slot[name=actions]",
  };

  constructor() {
    super();
    this.layout = "default";
  }

  stylesTemplate() {
    return html`${super.stylesTemplate()}
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-box-item.css"
      />`;
  }

  render() {
    return html`
      ${this.stylesTemplate()}
      <div class="box-container">
        <slot name="actions-start"></slot>
        <div class="box-content">
          ${this.label ? super.textTemplate() : html`<slot></slot>`}
        </div>
        <slot name="actions"></slot>
      </div>
    `;
  }
}
customElements.define("moz-box-item", MozBoxItem);
