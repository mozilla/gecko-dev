/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

window.MozXULElement.insertFTLIfNeeded("preview/linkPreview.ftl");

/**
 * Class representing a link preview onboarding card element.
 * This component is shown when onboarding state is true.
 *
 * @augments MozLitElement
 */
class LinkPreviewCardOnboarding extends MozLitElement {
  static get properties() {
    return {
      onboardingType: { type: String },
    };
  }

  constructor() {
    super();
    this.onboardingType = "shiftKey";
  }

  /**
   * Handles click on the "Try it now" button.
   *
   * @param {MouseEvent} _event - The click event.
   */
  handleTryLinkPreview(_event) {
    this.dispatchEvent(new CustomEvent("LinkPreviewCard:onboardingComplete"));
  }

  /**
   * Handles click on the "Close" button.
   *
   * @param {MouseEvent} _event - The click event.
   */
  handleCloseOnboarding(_event) {
    this.dispatchEvent(new CustomEvent("LinkPreviewCard:onboardingClose"));
  }

  /**
   * Renders the link preview onboarding element.
   *
   * @returns {import('lit').TemplateResult} The rendered HTML template.
   */
  render() {
    const titleL10nId =
      this.onboardingType === "longPress"
        ? "link-preview-onboarding-title-long-press"
        : "link-preview-onboarding-title-shift";
    const descriptionL10nId =
      this.onboardingType === "longPress"
        ? "link-preview-onboarding-description-long-press"
        : "link-preview-onboarding-description-shift";

    const imageSrc =
      this.onboardingType === "longPress"
        ? "chrome://browser/content/genai/assets/onboarding-link-preview-image-longpress.svg"
        : "chrome://browser/content/genai/assets/onboarding-link-preview-image-shift.svg";

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/genai/content/link-preview-card.css"
      />
      <link
        rel="stylesheet"
        href="chrome://browser/content/genai/content/link-preview-card-onboarding.css"
      />
      <div class="og-card onboarding">
        <div class="og-card-content">
          <img class="og-card-img" src=${imageSrc} alt="" />
          <h2 class="og-card-title" data-l10n-id=${titleL10nId}></h2>
          <p class="og-card-description" data-l10n-id=${descriptionL10nId}></p>
          <div class="reading-time-settings-container">
            <moz-button-group class="onboarding-button-group">
              <moz-button
                size="default"
                id="onboarding-close-button"
                @click=${this.handleCloseOnboarding}
                data-l10n-id="link-preview-onboarding-close"
              >
              </moz-button>
              <moz-button
                type="primary"
                @click=${this.handleTryLinkPreview}
                data-l10n-id="link-preview-onboarding-button"
              >
              </moz-button>
            </moz-button-group>
          </div>
        </div>
      </div>
    `;
  }
}

customElements.define(
  "link-preview-card-onboarding",
  LinkPreviewCardOnboarding
);
