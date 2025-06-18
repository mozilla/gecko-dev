/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozBoxBase } from "../lit-utils.mjs";
import { html } from "../vendor/lit.all.mjs";

window.MozXULElement?.insertFTLIfNeeded("toolkit/global/mozBoxBase.ftl");

/**
 * A link with a box-like shape that allows for custom title and description.
 *
 * @tagname moz-box-link
 * @property {string} label - Label for the button.
 * @property {string} description - Descriptive text for the button.
 * @property {string} iconSrc - The src for an optional icon.
 * @property {string} href - The href of the link.
 * @property {string} supportPage - Whether or not the link is to a support page.
 */
export default class MozBoxLink extends MozBoxBase {
  static properties = {
    href: { type: String },
    supportPage: { type: String, attribute: "support-page" },
  };

  constructor() {
    super();
    this.href = "";
    this.supportPage = "";
  }

  stylesTemplate() {
    const styles = super.stylesTemplate();
    return html`${styles}<link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-box-link.css"
      />`;
  }

  navIconTemplate() {
    return html`<img
      class="icon nav-icon"
      src="chrome://global/skin/icons/open-in-new.svg"
      role="presentation"
    />`;
  }

  render() {
    const template = html`${this.textTemplate()}${this.navIconTemplate()}`;
    const { supportPage } = this;

    return html`
      ${this.stylesTemplate()}
      ${supportPage
        ? html`<a
            class="button"
            is="moz-support-link"
            support-page=${supportPage}
            data-l10n-id="moz-box-link-anchor"
          >
            ${template}
          </a>`
        : html`<a
            class="button"
            href=${this.href}
            target="_blank"
            data-l10n-id="moz-box-link-anchor"
          >
            ${template}
          </a>`}
    `;
  }
}
customElements.define("moz-box-link", MozBoxLink);
