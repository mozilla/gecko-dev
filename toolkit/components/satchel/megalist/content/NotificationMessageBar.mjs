/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

class NotificationMessageBar extends MozLitElement {
  static properties = {
    notification: { type: Object },
    onDismiss: { type: Function },
    messageHandler: { type: Function },
  };

  #renderImportSuccess() {
    const { onLinkClick } = this.notification.commands;
    return html`
      <moz-message-bar
        @message-bar:user-dismissed=${() => this.onDismiss()}
        dismissable
        type="success"
        data-l10n-id="passwords-import-success-heading"
        data-l10n-attrs="heading"
        messageL10nId="passwords-import-success-message"
        .messageL10nArgs=${this.notification.l10nArgs}
      >
        <a
          slot="support-link"
          data-l10n-id="passwords-import-detailed-report"
          href="about:loginsimportreport"
          @click=${e => {
            e.preventDefault();
            this.messageHandler(onLinkClick);
          }}
        >
        </a>
        <moz-button
          slot="actions"
          data-l10n-id="passwords-import-success-button"
          @click=${() => this.onDismiss()}
        ></moz-button>
      </moz-message-bar>
    `;
  }

  #renderImportError() {
    const { onLinkClick, onRetry } = this.notification.commands;
    return html`
      <moz-message-bar
        @message-bar:user-dismissed=${() => this.onDismiss()}
        dismissable
        type="error"
        data-l10n-id="passwords-import-error-heading-and-message"
        data-l10n-attrs="heading, message"
      >
        <a
          slot="support-link"
          data-l10n-id="passwords-import-learn-more"
          href="https://support.mozilla.org/kb/import-login-data-file"
          @click=${() => this.messageHandler(onLinkClick)}
        >
        </a>
        <moz-button-group slot="actions">
          <moz-button
            type="primary"
            data-l10n-id="passwords-import-error-button-try-again"
            @click=${() => this.messageHandler(onRetry)}
          ></moz-button>
          <moz-button
            data-l10n-id="passwords-import-error-button-cancel"
            @click=${() => this.onDismiss()}
          ></moz-button>
        </moz-button-group>
      </moz-message-bar>
    `;
  }

  render() {
    switch (this.notification?.id) {
      case "import-success":
        return this.#renderImportSuccess();
      case "import-error":
        return this.#renderImportError();
      default:
        return "";
    }
  }
}

customElements.define("notification-message-bar", NotificationMessageBar);
