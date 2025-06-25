/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

/**
 * A simple badge element that can be used to indicate status or convey simple messages
 *
 * @tagname moz-badge
 * @property {string} label - Text to display on the badge
 * @property {string} iconSrc - The src for an optional icon shown next to the label
 * @property {string} title - The title of the badge, appears as a tooltip on hover
 */
export default class MozBadge extends MozLitElement {
  static properties = {
    label: { type: String, fluent: true },
    iconSrc: { type: String },
    title: { type: String, fluent: true, mapped: true },
  };

  constructor() {
    super();
    this.label = "";
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-badge.css"
      />
      <div class="moz-badge" title=${ifDefined(this.title)}>
        ${this.iconSrc
          ? html`<img class="moz-badge-icon" src=${this.iconSrc} role="presentation"></img>`
          : ""}
        <span class="moz-badge-label">${this.label}</span>
      </div>
    `;
  }
}
customElements.define("moz-badge", MozBadge);
