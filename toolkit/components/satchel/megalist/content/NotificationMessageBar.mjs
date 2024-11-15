/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  ifDefined,
  when,
  nothing,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

const actionButton = action => {
  return html` <moz-button
    slot=${ifDefined(action.slot)}
    type=${ifDefined(action.type)}
    data-l10n-id=${action.dataL10nId}
    @click=${action.onClick}
  ></moz-button>`;
};

const notificationShell = ({
  onDismiss,
  messageHandler,
  dataL10nId,
  dataL10nArgs,
  messageL10nId,
  messageL10nArgs,
  type,
  link,
  primaryAction,
  secondaryAction,
}) => {
  return html`
    <moz-message-bar
      @message-bar:user-dismissed=${onDismiss}
      dismissable
      type=${type}
      data-l10n-id=${dataL10nId}
      data-l10n-args=${ifDefined(dataL10nArgs)}
      messageL10nId=${ifDefined(messageL10nId)}
      .messageL10nArgs=${ifDefined(messageL10nArgs)}
    >
      ${when(
        link,
        () => html`<a
          slot="support-link"
          data-l10n-id=${link.dataL10nId}
          href=${link.url}
          @click=${e => {
            e.preventDefault();
            messageHandler(link.onClick);
          }}
        ></a>`,
        () => nothing
      )}
      ${when(
        secondaryAction,
        () =>
          html` <moz-button-group slot="actions">
            ${actionButton(primaryAction)} ${actionButton(secondaryAction)}
          </moz-button-group>`,
        () => html`${actionButton(primaryAction)}`
      )}
    </moz-message-bar>
  `;
};

class NotificationMessageBar extends MozLitElement {
  static properties = {
    notification: { type: Object },
    onDismiss: { type: Function },
    messageHandler: { type: Function },
  };

  #dispatchViewLoginEvent(guid) {
    this.dispatchEvent(
      new CustomEvent("view-login", {
        detail: {
          guid,
        },
      })
    );
  }

  #renderImportSuccess() {
    return html`${notificationShell({
      onDismiss: this.onDismiss,
      messageHandler: this.messageHandler,
      dataL10nId: "passwords-import-success-heading",
      messageL10nId: "passwords-import-success-message",
      messageL10nArgs: this.notification.l10nArgs,
      type: "success",
      link: {
        url: "about:loginsimportreport",
        dataL10nId: "passwords-import-detailed-report",
        onClick: this.notification.commands.onLinkClick,
      },
      primaryAction: {
        type: "primary",
        slot: "actions",
        dataL10nId: "passwords-import-success-button",
        onClick: this.onDismiss,
      },
    })}`;
  }

  #renderImportError() {
    return html`${notificationShell({
      onDismiss: this.onDismiss,
      messageHandler: this.messageHandler,
      dataL10nId: "passwords-import-error-heading-and-message",
      type: "error",
      link: {
        url: "https://support.mozilla.org/kb/import-login-data-file",
        dataL10nId: "passwords-import-learn-more",
        onClick: this.notification.commands.onLinkClick,
      },
      primaryAction: {
        type: "primary",
        dataL10nId: "passwords-import-error-button-try-again",
        onClick: () => this.messageHandler(this.notification.commands.onRetry),
      },
      secondaryAction: {
        dataL10nId: "passwords-import-error-button-cancel",
        onClick: this.onDismiss,
      },
    })}`;
  }

  #renderAddLoginSuccess() {
    return html`
      ${notificationShell({
        onDismiss: this.onDismiss,
        dataL10nId: "passwords-add-password-success-heading",
        dataL10nArgs: JSON.stringify(this.notification.l10nArgs),
        type: "success",
        primaryAction: {
          type: "primary",
          slot: "actions",
          dataL10nId: "passwords-add-password-success-button",
          onClick: () => {
            this.#dispatchViewLoginEvent(this.notification.guid);
            this.onDismiss();
          },
        },
      })}
    `;
  }

  #renderAddLoginAlreadyExistsWarning() {
    return html`
      ${notificationShell({
        onDismiss: this.onDismiss,
        dataL10nId: "passwords-password-already-exists-error-heading",
        dataL10nArgs: JSON.stringify(this.notification.l10nArgs),
        type: "warning",
        primaryAction: {
          type: "primary",
          slot: "actions",
          dataL10nId: "passwords-password-already-exists-error-button",
          onClick: () => {
            this.#dispatchViewLoginEvent(this.notification.guid);
            this.onDismiss();
          },
        },
      })}
    `;
  }

  render() {
    switch (this.notification?.id) {
      case "import-success":
        return this.#renderImportSuccess();
      case "import-error":
        return this.#renderImportError();
      case "add-login-success":
        return this.#renderAddLoginSuccess();
      case "add-login-already-exists-warning":
        return this.#renderAddLoginAlreadyExistsWarning();
      default:
        return "";
    }
  }
}

customElements.define("notification-message-bar", NotificationMessageBar);
