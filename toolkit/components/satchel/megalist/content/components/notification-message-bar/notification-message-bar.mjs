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
  dismissable = true,
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
      ?dismissable=${dismissable}
      type=${type}
      data-l10n-id=${dataL10nId}
      data-l10n-args=${ifDefined(dataL10nArgs)}
      messageL10nId=${ifDefined(messageL10nId)}
      .messageL10nArgs=${ifDefined(messageL10nArgs)}
    >
      ${when(
        link,
        () =>
          html`<a
            slot="support-link"
            data-l10n-id=${link.dataL10nId}
            href=${link.url}
            @click=${e => {
              e.preventDefault();
              messageHandler("OpenLink", { value: link.url });
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
        url: this.notification.url,
        dataL10nId: "passwords-import-detailed-report",
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
        url: this.notification.url,
        dataL10nId: "passwords-import-learn-more",
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

  #renderExportPasswordsSuccess() {
    return html`
      ${notificationShell({
        onDismiss: this.onDismiss,
        dataL10nId: "passwords-export-success-heading",
        type: "success",
        primaryAction: {
          type: "primary",
          slot: "actions",
          dataL10nId: "passwords-export-success-button",
          onClick: this.onDismiss,
        },
      })}
    `;
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

  #renderUpdateLoginSuccess() {
    return html`
      ${notificationShell({
        onDismiss: this.onDismiss,
        dataL10nId: "passwords-update-password-success-heading",
        dataL10nAttrs: "heading",
        type: "success",
        primaryAction: {
          type: "primary",
          slot: "actions",
          dataL10nId: "passwords-update-password-success-button",
          onClick: this.onDismiss,
        },
      })}
    `;
  }

  #renderDeleteLoginSuccess() {
    return html`
      ${notificationShell({
        onDismiss: this.onDismiss,
        dataL10nId: "passwords-delete-password-success-heading",
        dataL10nArgs: JSON.stringify(this.notification.l10nArgs),
        dataL10nAttrs: "heading",
        type: "success",
        primaryAction: {
          type: "primary",
          slot: "actions",
          dataL10nId: "passwords-delete-password-success-button",
          onClick: this.onDismiss,
        },
      })}
    `;
  }

  #renderDiscardChanges() {
    return html`${notificationShell({
      onDismiss: this.onDismiss,
      dataL10nId: "passwords-discard-changes-heading-and-message",
      type: "warning",
      primaryAction: {
        type: "destructive",
        dataL10nId: "passwords-discard-changes-confirm-button",
        onClick: () => {
          this.messageHandler("Cancel", {}, this.notification.passwordIndex);
          this.messageHandler("ConfirmDiscardChanges", {
            value: {
              fromSidebar: this.notification.fromSidebar,
            },
          });
        },
      },
      secondaryAction: {
        dataL10nId: "passwords-discard-changes-go-back-button",
        onClick: this.onDismiss,
      },
    })}`;
  }

  #renderBreachedOriginWarning() {
    return html`
      ${notificationShell({
        dismissable: false,
        onDismiss: this.onDismiss,
        messageHandler: this.messageHandler,
        dataL10nId: "passwords-breached-origin-heading-and-message",
        type: "error",
        link: {
          url: this.notification.url,
          dataL10nId: "passwords-breached-origin-link-message",
        },
        primaryAction: {
          slot: "actions",
          dataL10nId: "passwords-change-password-button",
          onClick: this.notification.onButtonClick,
        },
      })}
    `;
  }

  #renderNoUsernameWarning() {
    return html`
      ${notificationShell({
        dismissable: false,
        onDismiss: this.onDismiss,
        dataL10nId: "passwords-no-username-heading-and-message",
        type: "info",
        primaryAction: {
          slot: "actions",
          dataL10nId: "passwords-add-username-button",
          onClick: this.notification.onButtonClick,
        },
      })}
    `;
  }

  #renderVulnerablePasswordWarning() {
    return html`
      ${notificationShell({
        dismissable: false,
        onDismiss: this.onDismiss,
        messageHandler: this.messageHandler,
        dataL10nId: "passwords-vulnerable-password-heading-and-message",
        type: "warning",
        link: {
          url: this.notification.url,
          dataL10nId: "passwords-vulnerabe-password-link-message",
        },
        primaryAction: {
          slot: "actions",
          dataL10nId: "passwords-change-password-button",
          onClick: this.notification.onButtonClick,
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
      case "export-passwords-success":
        return this.#renderExportPasswordsSuccess();
      case "add-login-success":
        return this.#renderAddLoginSuccess();
      case "login-already-exists-warning":
        return this.#renderAddLoginAlreadyExistsWarning();
      case "update-login-success":
        return this.#renderUpdateLoginSuccess();
      case "delete-login-success":
        return this.#renderDeleteLoginSuccess();
      case "discard-changes":
        return this.#renderDiscardChanges();
      case "breached-origin-warning":
        return this.#renderBreachedOriginWarning();
      case "no-username-warning":
        return this.#renderNoUsernameWarning();
      case "vulnerable-password-warning":
        return this.#renderVulnerablePasswordWarning();
      default:
        return "";
    }
  }
}

customElements.define("notification-message-bar", NotificationMessageBar);
