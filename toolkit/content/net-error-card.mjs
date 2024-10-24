/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable import/no-unassigned-import */
/* eslint-env mozilla/remote-page */

import {
  getCSSClass,
  getHostName,
  getSubjectAltNames,
  getFailedCertificatesAsPEMString,
  recordSecurityUITelemetry,
} from "chrome://global/content/aboutNetErrorHelpers.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import "chrome://global/content/elements/moz-button-group.mjs";
import "chrome://global/content/elements/moz-button.mjs";
import "chrome://global/content/elements/moz-support-link.mjs";

const HOST_NAME = getHostName();

export class NetErrorCard extends MozLitElement {
  static properties = {
    hostname: { type: String },
    domainMismatchNames: { type: String },
    advancedShowing: { type: Boolean, reflect: true },
    certErrorDebugInfoShowing: { type: Boolean, reflect: true },
    certificateErrorText: { type: String },
  };

  static queries = {
    copyButtonTop: "#copyToClipboardTop",
    exceptionButton: "#exception-button",
    errorCode: "#errorCode",
    advancedContainer: ".advanced-container",
    advancedButton: "#advanced-button",
  };

  static ERROR_CODES = new Set([
    "SEC_ERROR_UNKNOWN_ISSUER",
    "SSL_ERROR_BAD_CERT_DOMAIN",
    "MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT",
    "SEC_ERROR_EXPIRED_CERTIFICATE",
    "SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE",
  ]);

  constructor() {
    super();

    this.domainMismatchNames = null;
    this.advancedShowing = false;
    this.certErrorDebugInfoShowing = false;
    this.certificateErrorText = null;
    this.domainMismatchNamesPromise = null;
    this.certificateErrorTextPromise = null;
  }

  async getUpdateComplete() {
    const result = await super.getUpdateComplete();

    if (this.domainMismatchNames && this.certificateErrorText) {
      return result;
    }

    await Promise.all([
      this.getDomainMismatchNames(),
      this.getCertificateErrorText(),
    ]);

    await Promise.all([
      this.domainMismatchNamesPromise,
      this.certificateErrorTextPromise,
    ]);

    return result;
  }

  connectedCallback() {
    super.connectedCallback();

    this.init();
  }

  firstUpdated() {
    // Dispatch this event so tests can detect that we finished loading the error page.
    document.dispatchEvent(
      new CustomEvent("AboutNetErrorLoad", { bubbles: true })
    );
  }

  init() {
    document.l10n.setAttributes(
      document.querySelector("title"),
      "fp-certerror-page-title"
    );

    this.failedCertInfo = document.getFailedCertSecurityInfo();

    this.hostname = HOST_NAME;
    const { port } = document.location;
    if (port && port != 443) {
      this.hostname += ":" + port;
    }

    if (getCSSClass() == "expertBadCert") {
      this.toggleAdvancedShowing();
    }
  }

  introContentTemplate() {
    switch (this.failedCertInfo.errorCodeString) {
      case "SEC_ERROR_UNKNOWN_ISSUER":
      case "SSL_ERROR_BAD_CERT_DOMAIN":
      case "SEC_ERROR_EXPIRED_CERTIFICATE":
      case "MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT":
        return html`<p
          data-l10n-id="fp-certerror-intro"
          data-l10n-args='{"hostname": "${this.hostname}"}'
        ></p>`;
      case "SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE":
        return html`<p
          data-l10n-id="fp-certerror-expired-intro"
          data-l10n-args='{"hostname": "${this.hostname}"}'
        ></p>`;
    }

    return null;
  }

  advancedContainerTemplate() {
    if (!this.advancedShowing) {
      return null;
    }

    let content;

    switch (this.failedCertInfo.errorCodeString) {
      case "SEC_ERROR_UNKNOWN_ISSUER": {
        content = this.advancedSectionTemplate({
          whyDangerousL10nId: "fp-certerror-unknown-issuer-why-dangerous-body",
          whatCanYouDoL10nId:
            "fp-certerror-unknown-issuer-what-can-you-do-body",
          learnMoreL10nId: "fp-learn-more-about-cert-issues",
          learnMoreSupportPage: "connection-not-secure",
          viewCert: true,
          viewDateTime: true,
        });
        break;
      }
      case "SSL_ERROR_BAD_CERT_DOMAIN": {
        if (!this.domainMismatchNames) {
          this.getDomainMismatchNames();
          return null;
        }

        content = this.advancedSectionTemplate({
          whyDangerousL10nId: "fp-certerror-bad-domain-why-dangerous-body",
          whyDangerousL10nArgs: {
            hostname: this.hostname,
            validHosts: this.domainMismatchNames ?? "",
          },
          whatCanYouDoL10nId: "fp-certerror-bad-domain-what-can-you-do-body",
          learnMoreL10nId: "fp-learn-more-about-secure-connection-failures",
          learnMoreSupportPage: "connection-not-secure",
          viewCert: true,
          viewDateTime: true,
        });
        break;
      }
      case "SEC_ERROR_EXPIRED_CERTIFICATE": {
        const notBefore = this.failedCertInfo.validNotBefore;
        const notAfter = this.failedCertInfo.validNotAfter;
        if (notBefore && Date.now() < notAfter) {
          content = this.advancedSectionTemplate({
            whyDangerousL10nId: "fp-certerror-not-yet-valid-why-dangerous-body",
            whyDangerousL10nArgs: {
              date: notBefore,
            },
            whatCanYouDoL10nId: "fp-certerror-expired-what-can-you-do-body",
            whatCanYouDoL10nArgs: {
              date: Date.now(),
            },
            learnMoreL10nId: "fp-learn-more-about-time-related-errors",
            learnMoreSupportPage: "time-errors",
            viewCert: true,
            viewDateTime: true,
          });
        } else {
          content = this.advancedSectionTemplate({
            whyDangerousL10nId: "fp-certerror-expired-why-dangerous-body",
            whyDangerousL10nArgs: {
              date: notAfter,
            },
            whatCanYouDoL10nId: "fp-certerror-expired-what-can-you-do-body",
            whatCanYouDoL10nArgs: {
              date: Date.now(),
            },
            learnMoreL10nId: "fp-learn-more-about-time-related-errors",
            learnMoreSupportPage: "time-errors",
            viewCert: true,
            viewDateTime: true,
          });
        }
        break;
      }
      case "MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT": {
        content = this.advancedSectionTemplate({
          whyDangerousL10nId: "fp-certerror-self-signed-why-dangerous-body",
          whatCanYouDoL10nId: "fp-certerror-self-signed-what-can-you-do-body",
          importantNote: "fp-certerror-self-signed-important-note",
          viewCert: true,
          viewDateTime: true,
        });
        break;
      }
      case "SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE": {
        const notAfter = this.failedCertInfo.validNotAfter;
        content = this.advancedSectionTemplate({
          whyDangerousL10nId: "fp-certerror-expired-why-dangerous-body",
          whyDangerousL10nArgs: {
            date: notAfter,
          },
          whatCanYouDoL10nId: "fp-certerror-expired-what-can-you-do-body",
          whatCanYouDoL10nArgs: {
            date: Date.now(),
          },
          learnMoreL10nId: "fp-learn-more-about-time-related-errors",
          learnMoreSupportPage: "time-errors",
          viewCert: true,
          viewDateTime: true,
        });
        break;
      }
    }

    return html`<div class="advanced-container">
      <h2 data-l10n-id="fp-certerror-advanced-title"></h2>
      ${content}
    </div>`;
  }

  advancedSectionTemplate(params) {
    let {
      whyDangerousL10nId,
      whyDangerousL10nArgs,
      whatCanYouDoL10nId,
      whatCanYouDoL10nArgs,
      importantNote,
      learnMoreL10nId,
      learnMoreSupportPage,
      viewCert,
      viewDateTime,
    } = params;
    return html`<p>
        ${whyDangerousL10nId
          ? html`<strong
                data-l10n-id="fp-certerror-why-site-dangerous"
              ></strong>
              <span
                data-l10n-id="${whyDangerousL10nId}"
                data-l10n-args=${JSON.stringify(whyDangerousL10nArgs)}
              ></span>`
          : null}
      </p>
      ${whatCanYouDoL10nId
        ? html`<p>
            <strong data-l10n-id="fp-certerror-what-can-you-do"></strong>
            <span
              data-l10n-id="${whatCanYouDoL10nId}"
              data-l10n-args=${JSON.stringify(whatCanYouDoL10nArgs)}
            ></span>
          </p>`
        : null}
      ${importantNote ? html`<p data-l10n-id="${importantNote}"></p>` : null}
      ${learnMoreL10nId
        ? html`<p>
            <a
              is="moz-support-link"
              support-page="${learnMoreSupportPage}"
              data-l10n-id="${learnMoreL10nId}"
              data-telemetry-id="learn_more_link"
              @click=${this.handleTelemetryClick}
            ></a>
          </p>`
        : null}
      ${viewCert
        ? html`<p>
            <a
              id="viewCertificate"
              data-l10n-id="fp-certerror-view-certificate-link"
              href="javascript:void(0)"
            ></a>
          </p>`
        : null}
      <p>
        <a
          id="errorCode"
          data-l10n-id="fp-cert-error-code"
          data-l10n-name="error-code-link"
          data-telemetry-id="error_code_link"
          data-l10n-args='{"error": "${this.failedCertInfo.errorCodeString}"}'
          @click=${this.toggleCertErrorDebugInfoShowing}
          href="#certificateErrorDebugInformation"
        ></a>
      </p>
      ${viewDateTime
        ? html`<p
            data-l10n-id="fp-datetime"
            data-l10n-args=${JSON.stringify({ datetime: Date.now() })}
          ></p>`
        : null}
      <moz-button
        id="exception-button"
        data-l10n-id="fp-certerror-override-exception-button"
        data-l10n-args=${JSON.stringify({ hostname: this.hostname })}
        data-telemetry-id="exception_button"
        @click=${this.handleProceedToUrlClick}
      ></moz-button>`;
  }

  async getDomainMismatchNames() {
    if (this.domainMismatchNamesPromise) {
      return;
    }

    this.domainMismatchNamesPromise = getSubjectAltNames(this.failedCertInfo);
    let subjectAltNames = await this.domainMismatchNamesPromise;
    this.domainMismatchNames = subjectAltNames.join(", ");
  }

  async getCertificateErrorText() {
    if (this.certificateErrorTextPromise) {
      return;
    }

    this.certificateErrorTextPromise = getFailedCertificatesAsPEMString();
    this.certificateErrorText = await this.certificateErrorTextPromise;
  }

  certErrorDebugInfoTemplate() {
    if (!this.certErrorDebugInfoShowing) {
      return null;
    }

    if (!this.certificateErrorText) {
      this.getCertificateErrorText();
      return null;
    }

    return html`<div
      id="certificateErrorDebugInformation"
      class="advanced-panel"
    >
      <moz-button
        id="copyToClipboardTop"
        data-telemetry-id="clipboard_button_top"
        data-l10n-id="neterror-copy-to-clipboard-button"
        @click=${this.copyCertErrorTextToClipboard}
      ></moz-button>
      <div id="certificateErrorText">${this.certificateErrorText}</div>
      <moz-button
        data-telemetry-id="clipboard_button_bot"
        data-l10n-id="neterror-copy-to-clipboard-button"
        @click=${this.copyCertErrorTextToClipboard}
      ></moz-button>
    </div>`;
  }

  handleGoBackClick(e) {
    this.handleTelemetryClick(e);
    RPMSendAsyncMessage("Browser:SSLErrorGoBack");
  }

  handleProceedToUrlClick(e) {
    this.handleTelemetryClick(e);
    const isPermanent =
      !RPMIsWindowPrivate() &&
      RPMGetBoolPref("security.certerrors.permanentOverride");
    document.addCertException(!isPermanent).then(
      () => {
        location.reload();
      },
      () => {}
    );
  }

  toggleAdvancedShowing(e) {
    if (e) {
      this.handleTelemetryClick(e);
    }

    this.advancedShowing = !this.advancedShowing;

    if (!this.advancedShowing) {
      return;
    }

    this.revealAdvancedContainer();
  }

  async revealAdvancedContainer() {
    await this.getUpdateComplete();

    // Toggling the advanced panel must ensure that the debugging
    // information panel is hidden as well, since it's opened by the
    // error code link in the advanced panel.
    this.certErrorDebugInfoShowing = false;

    // Reveal, but disabled (and grayed-out) for 3.0s.
    this.exceptionButton.disabled = true;

    // -

    if (this.resetReveal) {
      this.resetReveal(); // Reset if previous is pending.
    }
    let wasReset = false;
    this.resetReveal = () => {
      wasReset = true;
    };

    // Wait for 10 frames to ensure that the warning text is rendered
    // and gets all the way to the screen for the user to read it.
    // This is only ~0.160s at 60Hz, so it's not too much extra time that we're
    // taking to ensure that we're caught up with rendering, on top of the
    // (by default) whole second(s) we're going to wait based on the
    // security.dialog_enable_delay pref.
    // The catching-up to rendering is the important part, not the
    // N-frame-delay here.
    for (let i = 0; i < 10; i++) {
      await new Promise(requestAnimationFrame);
    }

    // Wait another Nms (default: 1000) for the user to be very sure. (Sorry speed readers!)
    const securityDelayMs = RPMGetIntPref("security.dialog_enable_delay", 1000);
    await new Promise(go => setTimeout(go, securityDelayMs));

    if (wasReset || !this.advancedShowing) {
      this.resetReveal = null;
      return;
    }

    // Enable and un-gray-out.
    this.exceptionButton.disabled = false;
  }

  async toggleCertErrorDebugInfoShowing(event) {
    this.handleTelemetryClick(event);
    event.preventDefault();

    this.certErrorDebugInfoShowing = !this.certErrorDebugInfoShowing;

    if (this.certErrorDebugInfoShowing) {
      await this.getUpdateComplete();
      this.copyButtonTop.scrollIntoView({
        block: "start",
        behavior: "smooth",
      });
      this.copyButtonTop.focus();
    }
  }

  copyCertErrorTextToClipboard(e) {
    this.handleTelemetryClick(e);
    navigator.clipboard.writeText(this.certificateErrorText);
  }

  handleTelemetryClick(event) {
    let target = event.originalTarget;
    if (!target.hasAttribute("data-telemetry-id")) {
      target = target.getRootNode().host;
    }
    let telemetryId = target.dataset.telemetryId;
    void recordSecurityUITelemetry(
      "securityUiCerterror",
      "click" +
        telemetryId
          .split("_")
          .map(word => word[0].toUpperCase() + word.slice(1))
          .join(""),
      this.failedCertInfo
    );
  }

  render() {
    if (!this.failedCertInfo) {
      return null;
    }

    return html`<link
        rel="stylesheet"
        href="chrome://global/skin/aboutNetError.css"
      />
      <article class="felt-privacy-container">
        <div class="img-container">
          <img src="chrome://global/skin/illustrations/security-error.svg" />
        </div>
        <div class="container">
          <h1 data-l10n-id="fp-certerror-body-title"></h1>
          ${this.introContentTemplate()}
          <moz-button-group
            ><moz-button
              type="primary"
              data-l10n-id="fp-certerror-return-to-previous-page-recommended-button"
              data-telemetry-id="return_button_adv"
              @click=${this.handleGoBackClick}
            ></moz-button
            ><moz-button
              id="advanced-button"
              data-l10n-id="${this.advancedShowing
                ? "fp-certerror-hide-advanced-button"
                : "fp-certerror-advanced-button"}"
              data-telemetry-id="advanced_button"
              @click=${this.toggleAdvancedShowing}
            ></moz-button
          ></moz-button-group>
          ${this.advancedContainerTemplate()}
          ${this.certErrorDebugInfoTemplate()}
        </div>
      </article>`;
  }
}
