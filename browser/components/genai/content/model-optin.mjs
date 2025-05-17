/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * Model Optin Component
 *
 * Displays a prompt allowing the user to opt in or out of a model download.
 * Can also show a progress bar while downloading.
 */
class ModelOptin extends MozLitElement {
  static properties = {
    headingL10nId: { type: String, fluent: true },
    headingIcon: { type: String },
    iconAtEnd: { type: Boolean },
    messageL10nId: { type: String, fluent: true },
    optinButtonL10nId: { type: String, fluent: true },
    optoutButtonL10nId: { type: String, fluent: true },
    footerMessageL10nId: { type: String, fluent: true },
    cancelDownloadButtonL10nId: { type: String, fluent: true },
    isLoading: { type: Boolean, reflect: true },
    progressStatus: { type: Number }, // Expected to be a number between 0 and 100
    isHidden: { type: Boolean },
  };

  static events = {
    confirm: "MlModelOptinConfirm",
    deny: "MlModelOptinDeny",
    cancelDownload: "MlModelOptinCancelDownload",
    messageLinkClick: "MlModelOptinMessageLinkClick",
    footerLinkClick: "MlModelOptinFooterLinkClick",
  };

  static eventBehaviors = {
    bubbles: true,
    composed: true,
  };

  constructor() {
    super();
    this.isLoading = false;
    this.isHidden = false;
    this.iconAtEnd = false;
    this.optinButtonL10nId = "genai-model-optin-continue";
    this.optoutButtonL10nId = "genai-model-optin-optout";
    this.cancelDownloadButtonL10nId = "genai-model-optin-cancel";
    this.footerMessageL10nId = "";
  }

  dispatch(event) {
    this.dispatchEvent(
      new CustomEvent(event, { bubbles: true, composed: true })
    );
  }

  handleConfirmClick() {
    this.dispatch(ModelOptin.events.confirm);
  }

  handleDenyClick() {
    this.dispatch(ModelOptin.events.deny);
    this.isHidden = true;
  }

  handleCancelDownloadClick() {
    this.dispatch(ModelOptin.events.cancelDownload);
    this.isLoading = false;
    this.progressStatus = undefined;
  }

  handleMessageLinkClick(e) {
    // ftl overrides the html, need to manually watch for event in parent.
    if (e.target.id !== "optin-message-link") {
      return;
    }
    this.dispatch(ModelOptin.events.messageLinkClick);
  }

  handleFooterLinkClick(e) {
    // ftl overrides the html, need to manually watch for event in parent.
    if (e.target.id !== "optin-footer-link") {
      return;
    }
    this.dispatch(ModelOptin.events.footerLinkClick);
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/genai/content/model-optin.css"
      />
      <section ?hidden=${this.isHidden} class="optin-wrapper">
        ${this.isLoading
          ? html`
              <div class="optin-progress-bar-wrapper">
                <progress
                  class="optin-progress-bar"
                  value=${this.progressStatus}
                  max="100"
                ></progress>
              </div>
            `
          : ""}

        <div class="optin-header-wrapper">
          <div class="optin-header">
            ${this.headingIcon
              ? html`<img
                  src=${this.headingIcon}
                  alt=${this.headingL10nId}
                  class="optin-heading-icon ${this.iconAtEnd
                    ? "icon-at-end"
                    : ""}"
                />`
              : ""}
            <h3 class="optin-heading" data-l10n-id=${this.headingL10nId}></h3>
          </div>
        </div>

        <p
          class="optin-message"
          data-l10n-id=${this.messageL10nId}
          @click=${this.handleMessageLinkClick}
        >
          <a id="optin-message-link" data-l10n-name="support" href="#"></a>
        </p>
        <slot></slot>

        ${this.isLoading
          ? html`
              <div class="optin-actions">
                <moz-button
                  size="small"
                  data-l10n-id=${this.cancelDownloadButtonL10nId}
                  @click=${this.handleCancelDownloadClick}
                >
                </moz-button>
              </div>
            `
          : html`
              <div class="optin-actions">
                <moz-button-group>
                  <moz-button
                    size="small"
                    id="optin-confirm-button"
                    type="primary"
                    data-l10n-id=${this.optinButtonL10nId}
                    @click=${this.handleConfirmClick}
                  >
                  </moz-button>
                  <moz-button
                    size="small"
                    id="optin-deny-button"
                    data-l10n-id=${this.optoutButtonL10nId}
                    @click=${this.handleDenyClick}
                  >
                  </moz-button>
                </moz-button-group>
              </div>
            `}
        ${!this.isLoading && this.footerMessageL10nId !== ""
          ? html`
              <p
                class="optin-footer-message"
                data-l10n-id=${this.footerMessageL10nId}
                @click=${this.handleFooterLinkClick}
              >
                <a
                  id="optin-footer-link"
                  data-l10n-name="settings"
                  href="#"
                ></a>
              </p>
            `
          : ""}
      </section>
    `;
  }
}
customElements.define("model-optin", ModelOptin);
