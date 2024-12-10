/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import { MozBaseInputElement } from "../lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-label.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-support-link.mjs";

/**
 * A checkbox input with a label.
 *
 * @tagname moz-checkbox
 * @property {string} label - The text of the label element
 * @property {string} name - The name of the checkbox input control
 * @property {string} value - The value of the checkbox input control
 * @property {boolean} checked - The state of the checkbox element,
 *  also controls whether the checkbox is initially rendered as
 *  being checked.
 * @property {boolean} disabled - The disabled state of the checkbox input
 * @property {string} iconSrc - The src for an optional icon
 * @property {string} description - The text for the description element that helps describe the checkbox
 * @property {string} supportPage - Name of the SUMO support page to link to.
 */
export default class MozCheckbox extends MozBaseInputElement {
  static properties = {
    checked: { type: Boolean, reflect: true },
  };

  constructor() {
    super();
    this.checked = false;
  }

  /**
   * Handles click events and keeps the checkbox checked value in sync
   *
   * @param {Event} event
   * @memberof MozCheckbox
   */
  handleStateChange(event) {
    this.checked = event.target.checked;
  }

  /**
   * Dispatches an event from the host element so that outside
   * listeners can react to these events
   *
   * @param {Event} event
   * @memberof MozCheckbox
   */
  redispatchEvent(event) {
    let newEvent = new Event(event.type, event);
    this.dispatchEvent(newEvent);
  }

  inputTemplate() {
    return html`
      <input
        id="input"
        type="checkbox"
        name=${this.name}
        value=${this.value}
        .checked=${this.checked}
        @click=${this.handleStateChange}
        @change=${this.redispatchEvent}
        .disabled=${this.disabled}
        aria-describedby="description"
        accesskey=${ifDefined(this.accessKey)}
      />
    `;
  }
}
customElements.define("moz-checkbox", MozCheckbox);
