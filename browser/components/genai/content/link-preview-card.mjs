/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import {
  createRef,
  html,
  ref,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(
  lazy,
  "numberFormat",
  () => new Services.intl.NumberFormat()
);

ChromeUtils.defineLazyGetter(
  lazy,
  "pluralRules",
  () => new Services.intl.PluralRules()
);

ChromeUtils.importESModule(
  "chrome://browser/content/genai/content/model-optin.mjs",
  {
    global: "current",
  }
);

window.MozXULElement.insertFTLIfNeeded("preview/linkPreview.ftl");

/**
 * Class representing a link preview element.
 *
 * @augments MozLitElement
 */
class LinkPreviewCard extends MozLitElement {
  static AI_ICON = "chrome://global/skin/icons/highlights.svg";
  // Number of placeholder rows to show when loading
  static PLACEHOLDER_COUNT = 3;

  static properties = {
    collapsed: { type: Boolean },
    generating: { type: Number }, // 0 = off, 1-4 = generating & dots state
    isMissingDataErrorState: { type: Boolean },
    generationError: { type: Object }, // null = no error, otherwise contains error info
    keyPoints: { type: Array },
    optin: { type: Boolean },
    pageData: { type: Object },
    progress: { type: Number }, // -1 = off, 0-100 = download progress
    regionSupported: { type: Boolean },
  };

  constructor() {
    super();
    this.collapsed = false;
    this.generationError = null;
    this.isMissingDataErrorState = false;
    this.keyPoints = [];
    this.optin = false;
    this.optinRef = createRef();
    this.progress = -1;
    this.regionSupported = true;
  }

  /**
   * Handles click events on the settings button.
   *
   * Prevents the default event behavior and opens Firefox's preferences
   * page with the link preview settings section focused.
   *
   * @param {MouseEvent} _event - The click event from the settings button.
   */
  handleSettingsClick(_event) {
    const win = this.ownerGlobal;
    win.openPreferences("general-link-preview");
    this.dispatchEvent(
      new CustomEvent("LinkPreviewCard:dismiss", {
        detail: "settings",
      })
    );
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

    const anchor = event.target.closest("a");
    const url = anchor.href;

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

    this.dispatchEvent(
      new CustomEvent("LinkPreviewCard:dismiss", {
        detail: event.target.dataset.source ?? "error",
      })
    );
  }

  /**
   * Handles retry request for key points generation.
   *
   * @param {MouseEvent} event - The click event.
   */
  handleRetry(event) {
    event.preventDefault();
    // Dispatch retry event to be handled by LinkPreview.sys.mjs
    this.dispatchEvent(new CustomEvent("LinkPreviewCard:retry"));
  }

  /**
   * Toggles the expanded state of the key points section
   *
   * @param {MouseEvent} _event - The click event
   */
  toggleKeyPoints(_event) {
    Services.prefs.setBoolPref(
      "browser.ml.linkPreview.collapsed",
      !this.collapsed
    );

    // When expanded, if there are existing key points, we won't trigger
    // another generation
    if (!this.collapsed) {
      this.dispatchEvent(new CustomEvent("LinkPreviewCard:generate"));
    }
  }

  updated(properties) {
    if (this.optinRef.value) {
      this.optinRef.value.headingIcon = LinkPreviewCard.AI_ICON;
    }

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
   * Get the appropriate Fluent ID for the error message based on the error state.
   *
   * @returns {string} The Fluent ID for the error message.
   */
  get errorMessageL10nId() {
    if (this.isMissingDataErrorState) {
      return "link-preview-generation-error-missing-data";
    } else if (this.generationError) {
      return "link-preview-generation-error-unexpected";
    }
    return "";
  }

  /**
   * Renders the error generation card for when we have a generation error.
   *
   * @returns {import('lit').TemplateResult} The error generation card HTML
   */
  renderErrorGenerationCard() {
    // Only show the retry link if we have a generation error that's not a memory error
    const showRetryLink =
      this.generationError &&
      this.generationError.name !== "NotEnoughMemoryError";

    return html`
      <div class="ai-content">
        <p class="og-error-message-container">
          <span
            class="og-error-message"
            data-l10n-id=${this.errorMessageL10nId}
          ></span>
          ${showRetryLink
            ? html`
                <span class="retry-link">
                  <a
                    href="#"
                    @click=${this.handleRetry}
                    data-l10n-id="link-preview-generation-retry"
                  ></a>
                </span>
              `
            : ""}
        </p>
      </div>
    `;
  }

  /**
   * Renders a placeholder generation card for the opt-in mode,
   * showing only loading animations without real content.
   *
   * @returns {import('lit').TemplateResult} The opt-in placeholder card HTML
   */
  renderOptInPlaceholderCard() {
    return html`
      <div class="ai-content">
        <h3
          class="keypoints-header"
          @click=${this._handleOptinDeny}
          tabindex="0"
          role="button"
          aria-expanded=${!this.collapsed}
        >
          <div class="chevron-icon"></div>
          Key points
          <img
            class="icon"
            xmlns="http://www.w3.org/1999/xhtml"
            role="presentation"
            src="chrome://global/skin/icons/highlights.svg"
          />
        </h3>
        <div class="keypoints-content ${this.collapsed ? "hidden" : ""}">
          <ul class="keypoints-list">
            ${
              /* Always show 3 placeholder loading items */
              Array(LinkPreviewCard.PLACEHOLDER_COUNT)
                .fill()
                .map(
                  () =>
                    html` <li class="content-item loading static">
                      <div></div>
                      <div></div>
                      <div></div>
                    </li>`
                )
            }
          </ul>
          ${this.renderModelOptIn()}
        </div>
      </div>
    `;
  }

  /**
   * Renders the normal generation card for displaying key points.
   *
   * @param {string} pageUrl - URL of the page being previewed
   * @returns {import('lit').TemplateResult} The normal generation card HTML
   */
  renderNormalGenerationCard(pageUrl) {
    // Extract the links section into its own variable
    const linksSection = html`
      <p>Key points are AI-generated and may have mistakes.</p>
    `;

    return html`
      <div class="ai-content">
        <h3
          class="keypoints-header"
          @click=${this.toggleKeyPoints}
          tabindex="0"
          role="button"
          aria-expanded=${!this.collapsed}
        >
          <div class="chevron-icon"></div>
          Key points
          <img
            class="icon"
            xmlns="http://www.w3.org/1999/xhtml"
            role="presentation"
            src="chrome://global/skin/icons/highlights.svg"
          />
        </h3>
        <div class="keypoints-content ${this.collapsed ? "hidden" : ""}">
          <ul class="keypoints-list">
            ${
              /* All populated content items */
              this.keyPoints.map(
                item => html`<li class="content-item">${item}</li>`
              )
            }
            ${
              /* Loading placeholders with three divs each */
              this.generating
                ? Array(
                    Math.max(
                      0,
                      LinkPreviewCard.PLACEHOLDER_COUNT - this.keyPoints.length
                    )
                  )
                    .fill()
                    .map(
                      () =>
                        html` <li class="content-item loading">
                          <div></div>
                          <div></div>
                          <div></div>
                        </li>`
                    )
                : []
            }
          </ul>
          ${!this.generating
            ? html`
                <div class="visit-link-container">
                  <a
                    @click=${this.handleLink}
                    data-source="visit"
                    href=${pageUrl}
                    class="visit-link"
                  >
                    Visit page
                    <img
                      class="icon"
                      xmlns="http://www.w3.org/1999/xhtml"
                      role="presentation"
                      src="chrome://global/skin/icons/open-in-new.svg"
                    />
                  </a>
                </div>
              `
            : ""}
          ${this.progress >= 0
            ? html`
                <p>First-time setup • <strong>${this.progress}%</strong></p>
                <p>You'll see key points more quickly next time.</p>
              `
            : ""}
          ${!this.generating
            ? html`
                <hr />
                ${linksSection}
              `
            : ""}
        </div>
      </div>
    `;
  }

  /**
   * Renders the model opt-in component that prompts users to optin to AI features.
   * This component allows users to opt in or out of the link preview AI functionality
   * and includes a support link for more information.
   *
   * @returns {import('lit').TemplateResult} The model opt-in component HTML
   */
  renderModelOptIn() {
    return html`
      <model-optin
        ${ref(this.optinRef)}
        headingIcon=${LinkPreviewCard.AI_ICON}
        headingL10nId="link-preview-optin-title"
        iconAtEnd
        messageL10nId="link-preview-optin-message"
        @MlModelOptinConfirm=${this._handleOptinConfirm}
        @MlModelOptinDeny=${this._handleOptinDeny}
      >
      </model-optin>
    `;
  }

  /**
   * Handles the user confirming the opt-in prompt for link preview.
   * Sets preference values to enable the feature, hides the prompt for future sessions,
   * and triggers a retry to generate the preview.
   */
  _handleOptinConfirm() {
    Services.prefs.setBoolPref("browser.ml.linkPreview.optin", true);

    this.dispatchEvent(new CustomEvent("LinkPreviewCard:generate"));
  }

  /**
   * Handles the user denying the opt-in prompt for link preview.
   * Sets preference values to disable the feature and hides
   * the prompt for future sessions.
   */
  _handleOptinDeny() {
    Services.prefs.setBoolPref("browser.ml.linkPreview.optin", false);
    Services.prefs.setBoolPref("browser.ml.linkPreview.collapsed", true);

    Glean.genaiLinkpreview.cardAiConsent.record({ option: "cancel" });
  }

  /**
   * Renders the appropriate content card based on state.
   *
   * @param {string} pageUrl - URL of the page being previewed
   * @returns {import('lit').TemplateResult} The content card HTML
   */
  renderKeyPointsSection(pageUrl) {
    if (!this.regionSupported) {
      return "";
    }

    // Determine if there's any generation error state
    const isGenerationError =
      this.isMissingDataErrorState || this.generationError;

    // If we should show the opt-in prompt, show our special placeholder card
    if (!this.optin && !this.collapsed) {
      return this.renderOptInPlaceholderCard();
    }

    if (isGenerationError) {
      return this.renderErrorGenerationCard(pageUrl);
    }

    // Always render the ai-content, otherwise we won't have header to expand/collapse
    return this.renderNormalGenerationCard(pageUrl);
  }

  /**
   * Renders the link preview element.
   *
   * @returns {import('lit').TemplateResult} The rendered HTML template.
   */
  render() {
    const articleData = this.pageData?.article || {};
    const pageUrl = this.pageData?.url || "about:blank";
    const siteName =
      articleData.siteName || this.pageData?.urlComponents?.domain || "";

    const { title, description, imageUrl } = this.pageData.meta;

    const readingTimeMinsFast = articleData.readingTimeMinsFast || "";
    const readingTimeMinsSlow = articleData.readingTimeMinsSlow || "";
    const readingTimeMinsFastStr =
      lazy.numberFormat.format(readingTimeMinsFast);
    const readingTimeRange = lazy.numberFormat.formatRange(
      readingTimeMinsFast,
      readingTimeMinsSlow
    );

    // Check if both metadata and article text content are missing
    const isMissingAllContent = !description && !articleData.textContent;

    const filename = this.pageData?.urlComponents?.filename;

    // Error Link Preview card UI: A simplified version of the preview card showing only an error message
    // and a link to visit the URL. This is a fallback UI for cases when we don't have
    // enough metadata to generate a useful preview.
    const errorCard = html`
      <div class="og-card">
        <div class="og-card-content">
          <div class="og-error-content">
            <p
              class="og-error-message"
              data-l10n-id="link-preview-error-message"
            ></p>
            <a
              class="og-card-title"
              @click=${this.handleLink}
              data-l10n-id="link-preview-visit-link"
              href=${pageUrl}
            ></a>
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
          ${imageUrl.startsWith("https://")
            ? html` <img class="og-card-img" src=${imageUrl} alt=${title} /> `
            : ""}
          ${siteName
            ? html`
                <div class="page-info-and-card-setting-container">
                  <span class="site-name">${siteName}</span>
                </div>
              `
            : ""}
          <h2 class="og-card-title">
            <a @click=${this.handleLink} data-source="title" href=${pageUrl}
              >${title || filename}</a
            >
          </h2>
          ${description
            ? html`<p class="og-card-description">${description}</p>`
            : ""}
          ${readingTimeMinsFast && readingTimeMinsSlow
            ? html`
                <div class="reading-time-settings-container">
                  <div
                    class="og-card-reading-time"
                    data-l10n-id="link-preview-reading-time"
                    data-l10n-args=${JSON.stringify({
                      range:
                        readingTimeMinsFast === readingTimeMinsSlow
                          ? `~${readingTimeMinsFastStr}`
                          : `${readingTimeRange}`,
                      rangePlural:
                        readingTimeMinsFast === readingTimeMinsSlow
                          ? lazy.pluralRules.select(readingTimeMinsFast)
                          : lazy.pluralRules.selectRange(
                              readingTimeMinsFast,
                              readingTimeMinsSlow
                            ),
                    })}
                  ></div>
                  <moz-button
                    type="icon ghost"
                    iconSrc="chrome://global/skin/icons/settings.svg"
                    data-l10n-id="link-preview-settings-button"
                    data-l10n-attrs="title"
                    @click=${this.handleSettingsClick}
                  >
                  </moz-button>
                </div>
              `
            : ""}
        </div>
        ${this.renderKeyPointsSection(pageUrl)}
      </div>
    `;

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/genai/content/link-preview-card.css"
      />
      ${isMissingAllContent ? errorCard : normalCard}
    `;
  }
}

customElements.define("link-preview-card", LinkPreviewCard);
