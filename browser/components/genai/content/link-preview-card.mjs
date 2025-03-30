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

// TODO put in actual link probably same as labs bug 1951144
const FEEDBACK_LINK =
  "https://docs.google.com/spreadsheets/d/1hsG7UXGJRN8D4ViaETICDyA0gbBArzmib1qTylmIu8M";

/**
 * Class representing a link preview element.
 *
 * @augments MozLitElement
 */
class LinkPreviewCard extends MozLitElement {
  static properties = {
    generating: { type: Number }, // 0 = off, 1-4 = generating & dots state
    keyPoints: { type: Array },
    pageData: { type: Object },
    showWait: { type: Boolean },
    progressPercentage: { type: Number },
  };

  constructor() {
    super();
    this.keyPoints = [];
    this.progress = 0;
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
        // Count up to 4 so that we can show 0 to 3 dots.
        this.dotsTimeout = setTimeout(
          () => (this.generating = (this.generating % 4) + 1),
          500
        );
      } else {
        // Setting to false or 0 means we're done generating.
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
      "This link can’t be previewed";

    const description =
      articleData.excerpt ||
      metaData["og:description"] ||
      metaData["twitter:description"] ||
      metaData.description ||
      "No Reason. Just ’cause. (better error handling incoming)";

    let imageUrl = metaData["og:image"] || metaData["twitter:image:src"] || "";
    if (!imageUrl.startsWith("https://")) {
      imageUrl = "";
    }
    const readingTimeMinsFast = articleData.readingTimeMinsFast || "";
    const readingTimeMinsSlow = articleData.readingTimeMinsSlow || "";

    let displayProgressPercentage = this.progressPercentage;
    // Handle non-finite values (NaN, Infinity) by defaulting to 100%
    if (!Number.isFinite(displayProgressPercentage)) {
      displayProgressPercentage = 100;
    }

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/genai/content/link-preview-card.css"
      />
      <div class="og-card">
        <div class="og-card-content">
          ${imageUrl
            ? html` <img class="og-card-img" src=${imageUrl} alt=${title} /> `
            : ""}
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
          ${readingTimeMinsFast && readingTimeMinsSlow
            ? html`<div class="og-card-reading-time">
                ${readingTimeMinsFast === readingTimeMinsSlow
                  ? `${readingTimeMinsFast} min${readingTimeMinsFast > 1 ? "s" : ""} reading time`
                  : `${readingTimeMinsFast}-${readingTimeMinsSlow} mins reading time`}
              </div>`
            : ""}
        </div>
        ${this.generating || this.keyPoints.length
          ? html`
              <div class="ai-content">
                <h3>
                  ${this.generating
                    ? "Generating key points" + ".".repeat(this.generating - 1)
                    : "Key points"}
                </h3>
                <ul>
                  ${this.keyPoints.map(item => html`<li>${item}</li>`)}
                </ul>
                <hr />
                ${this.showWait
                  ? html`
                      <p>
                        ${this.progressPercentage > 0
                          ? html`Preparing Firefox •
                              <strong>${displayProgressPercentage}%</strong>`
                          : ""}
                      </p>
                      <p>
                        This may take a moment the first time you preview a
                        link. Key points should appear more quickly next time.
                      </p>
                    `
                  : ""}
                <p>
                  Key points are AI-generated and may have mistakes.
                  <a @click=${this.handleLink} href=${FEEDBACK_LINK}>
                    Foxfooding feedback
                  </a>
                </p>
                <p>
                  <a @click=${this.handleLink} href=${pageUrl}>
                    Visit original page
                  </a>
                </p>
              </div>
            `
          : ""}
      </div>
    `;
  }
}

customElements.define("link-preview-card", LinkPreviewCard);
