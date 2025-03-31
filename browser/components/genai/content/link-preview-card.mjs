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
const FEEDBACK_LINK = "https://connect.mozilla.org/";

/**
 * Class representing a link preview element.
 *
 * @augments MozLitElement
 */
class LinkPreviewCard extends MozLitElement {
  // Error message to display when we can't generate a preview
  static ERROR_CARD_MESSAGE = "We can't preview this link";
  // Text for the link to visit the original URL when in error state
  static VISIT_LINK_TEXT = "Visit link";

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
   * Extracts and validates essential metadata for link preview.
   *
   * This utility function processes the metadata object to extract the title and
   * description from various potential sources (Open Graph, Twitter, HTML).
   * It also determines if there's sufficient metadata to generate a meaningful preview.
   *
   * @param {object} metaData - The metadata object containing page information
   * @returns {object} An object containing:
   *  - isMissingMetadata {boolean} - True if either title or description is missing
   *  - title {string} - The extracted page title or empty string if none found
   *  - description {string} - The extracted page description or empty string if none found
   */
  extractMetadataContent(metaData = {}) {
    const title =
      metaData["og:title"] ||
      metaData["twitter:title"] ||
      metaData["html:title"] ||
      "";

    const description =
      metaData["og:description"] ||
      metaData["twitter:description"] ||
      metaData.description ||
      "";

    const isMissingMetadata = !title || !description;

    return {
      isMissingMetadata,
      title,
      description,
    };
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

    const { isMissingMetadata, title, description } =
      this.extractMetadataContent(metaData);

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

    // Error Link Preview card UI: A simplified version of the preview card showing only an error message
    // and a link to visit the URL. This is a fallback UI for cases when we don't have
    // enough metadata to generate a useful preview.
    const errorCard = html`
      <div class="og-card">
        <div class="og-card-content">
          <div class="og-error-content">
            <p class="og-error-message">
              ${LinkPreviewCard.ERROR_CARD_MESSAGE}
            </p>
            <a class="og-card-title" @click=${this.handleLink} href=${pageUrl}>
              ${LinkPreviewCard.VISIT_LINK_TEXT}
            </a>
          </div>
        </div>
      </div>
    `;

    // Normal Link Preview card UI: Shown when we have sufficient metadata (at least title and description)
    // Displays rich preview information including optional elements like site name, image,
    // reading time, and AI-generated key points if available
    const normalCard = html`
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

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/genai/content/link-preview-card.css"
      />
      ${isMissingMetadata ? errorCard : normalCard}
    `;
  }
}

customElements.define("link-preview-card", LinkPreviewCard);
