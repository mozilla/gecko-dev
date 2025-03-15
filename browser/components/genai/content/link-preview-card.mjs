/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
});

/**
 * Class representing a link preview element.
 *
 * @augments MozLitElement
 */
class LinkPreviewCard extends MozLitElement {
  static properties = {
    generating: { type: Number },
    keyPoints: { type: Array },
    pageData: { type: Object },
  };

  constructor() {
    super();
    this.keyPoints = [];
  }

  addKeyPoint(text) {
    this.keyPoints.push(text);
    this.requestUpdate();
  }

  /**
   * Handles click events on the <a> element.
   *
   * @param {MouseEvent} event - The click event.
   */
  handleLink(event) {
    event.preventDefault();

    const url = event.target.href;
    const win = this.ownerGlobal;
    const params = {
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        {}
      ),
    };

    // Determine where to open the link based on the event (e.g., new tab,
    // current tab)
    const where = lazy.BrowserUtils.whereToOpenLink(event, false, true);
    win.openLinkIn(url, where, params);

    this.dispatchEvent(new CustomEvent("LinkPreviewCard:dismiss"));
  }

  updated(properties) {
    if (properties.has("generating")) {
      if (this.generating > 0) {
        this.dotsTimeout = setTimeout(
          () => (this.generating = (this.generating % 3) + 1),
          500
        );
      } else {
        clearTimeout(this.dotsTimeout);
      }
    }
  }

  /**
   * Renders the link preview element.
   *
   * @returns {import('lit').TemplateResult} The rendered HTML template.
   */
  render() {
    const articleData = this.pageData?.article || {};
    const metaData = this.pageData?.metaInfo || {};
    const pageUrl = this.pageData?.url || "about:blank";

    const siteName = articleData.siteName || "";

    const title =
      metaData["og:title"] ||
      metaData["twitter:title"] ||
      metaData["html:title"] ||
      pageUrl ||
      "";

    const description =
      articleData.excerpt ||
      metaData["og:description"] ||
      metaData["twitter:description"] ||
      metaData.description ||
      "";

    const imageUrl =
      metaData["og:image"] || metaData["twitter:image:src"] || "";

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/genai/content/link-preview-card.css"
      />
      <div class="og-card">
        ${imageUrl
          ? html`
              <div class="og-card-img">
                <img src=${imageUrl} alt=${title} />
              </div>
            `
          : ""}
        <div class="og-card-content">
          ${siteName
            ? html`
                <div class="page-info-and-card-setting-container">
                  <span class="site-name">${siteName}</span>
                </div>
              `
            : ""}
          ${title
            ? html`
                <h2 class="og-card-title">
                  <a @click=${this.handleLink} href=${pageUrl}>${title}</a>
                </h2>
              `
            : ""}
          ${description
            ? html`<p class="og-card-description">${description}</p>`
            : ""}
        </div>
        ${this.generating || this.keyPoints.length
          ? html`
              <div class="ai-content">
                <h3>
                  Generat${this.generating ? "ing" : "ed"} key
                  points${".".repeat(this.generating)}
                </h3>
                <ul>
                  ${this.keyPoints.map(item => html`<li>${item}</li>`)}
                </ul>
                <hr />
                <p>AI-generated content may be inaccurate</p>
              </div>
            `
          : ""}
      </div>
    `;
  }
}

customElements.define("link-preview-card", LinkPreviewCard);
