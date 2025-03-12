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

const recordNotificationInteraction = (notificationDetail, actionType) => {
  Glean.contextualManager.notificationInteraction.record({
    notification_detail: notificationDetail.replaceAll("-", "_"),
    action_type: actionType,
  });
};

const actionButton = (action, id) => {
  return html` <moz-button
    slot=${ifDefined(action.slot)}
    type=${ifDefined(action.type)}
    data-l10n-id=${action.dataL10nId}
    @click=${() => {
      recordNotificationInteraction(action.telemetryId, action.telemetryType);
      action.onClick();
    }}
    id=${id}
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
  notificationId,
}) => {
  return html`
    <moz-message-bar
      @message-bar:user-dismissed=${ifDefined(onDismiss)}
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
            part="support-link"
            slot="support-link"
            data-l10n-id=${link.dataL10nId}
            href=${link.url}
            @click=${e => {
              e.preventDefault();
              recordNotificationInteraction(notificationId, "open_link");
              messageHandler("OpenLink", {
                value: link.url,
              });
            }}
          ></a>`,
        () => nothing
      )}
      ${when(
        secondaryAction,
        () =>
          html` <moz-button-group slot="actions">
            ${actionButton(primaryAction, "primary-action")}
            ${actionButton(secondaryAction, "secondary-action")}
          </moz-button-group>`,
        () => html`${actionButton(primaryAction, "primary-action")}`
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

  #handleDismiss() {
    recordNotificationInteraction(this.notification.id, "dismiss");
    this.onDismiss();
  }

  #renderImportSuccess() {
    return html`${notificationShell({
      onDismiss: this.#handleDismiss,
      messageHandler: this.messageHandler,
      dataL10nId: "contextual-manager-passwords-import-success-heading",
      messageL10nId: "contextual-manager-passwords-import-success-message",
      messageL10nArgs: this.notification.l10nArgs,
      type: "success",
      link: {
        url: this.notification.url,
        dataL10nId: "contextual-manager-passwords-import-detailed-report",
      },
      primaryAction: {
        type: "primary",
        telemetryType: "dismiss",
        telemetryId: this.notification.id,
        slot: "actions",
        dataL10nId: "contextual-manager-passwords-import-success-button",
        onClick: this.onDismiss,
      },
      notificationId: this.notification.id,
    })} `;
  }

  #renderImportError() {
    return html`${notificationShell({
      onDismiss: this.#handleDismiss,
      messageHandler: this.messageHandler,
      dataL10nId:
        "contextual-manager-passwords-import-error-heading-and-message",
      type: "error",
      link: {
        url: this.notification.url,
        dataL10nId: "contextual-manager-passwords-import-learn-more",
      },
      primaryAction: {
        type: "primary",
        telemetryType: "import",
        telemetryId: this.notification.id,
        dataL10nId:
          "contextual-manager-passwords-import-error-button-try-again",
        onClick: () => this.messageHandler(this.notification.commands.onRetry),
      },
      secondaryAction: {
        telemetryType: "dismiss",
        telemetryId: this.notification.id,
        dataL10nId: "contextual-manager-passwords-import-error-button-cancel",
        onClick: this.onDismiss,
      },
      notificationId: this.notification.id,
    })}`;
  }

  #renderExportPasswordsSuccess() {
    return html`
      ${notificationShell({
        onDismiss: this.#handleDismiss,
        dataL10nId: "contextual-manager-passwords-export-success-heading",
        type: "success",
        primaryAction: {
          type: "primary",
          telemetryType: "dismiss",
          telemetryId: this.notification.id,
          slot: "actions",
          dataL10nId: "contextual-manager-passwords-export-success-button",
          onClick: this.onDismiss,
        },
        notificationId: this.notification.id,
      })}
    `;
  }

  #renderAddLoginSuccess() {
    return html`
      ${notificationShell({
        onDismiss: this.#handleDismiss,
        dataL10nId: "contextual-manager-passwords-add-password-success-heading",
        dataL10nArgs: JSON.stringify(this.notification.l10nArgs),
        type: "success",
        primaryAction: {
          type: "primary",
          telemetryType: "nav_record",
          telemetryId: this.notification.id,
          slot: "actions",
          dataL10nId:
            "contextual-manager-passwords-add-password-success-button",
          onClick: () => {
            this.#dispatchViewLoginEvent(this.notification.guid);
            this.onDismiss();
          },
        },
        notificationId: this.notification.id,
      })}
    `;
  }

  #renderAddLoginAlreadyExistsWarning() {
    return html`
      ${notificationShell({
        onDismiss: this.#handleDismiss,
        dataL10nId:
          "contextual-manager-passwords-password-already-exists-error-heading",
        dataL10nArgs: JSON.stringify(this.notification.l10nArgs),
        type: "warning",
        primaryAction: {
          type: "primary",
          telemetryType: "nav_record",
          telemetryId: this.notification.id,
          slot: "actions",
          dataL10nId:
            "contextual-manager-passwords-password-already-exists-error-button",
          onClick: () => {
            this.#dispatchViewLoginEvent(this.notification.guid);
            this.onDismiss();
          },
        },
        notificationId: this.notification.id,
      })}
    `;
  }

  #renderUpdateLoginSuccess() {
    return html`
      ${notificationShell({
        onDismiss: this.#handleDismiss,
        dataL10nId:
          "contextual-manager-passwords-update-password-success-heading",
        dataL10nAttrs: "heading",
        type: "success",
        primaryAction: {
          type: "primary",
          telemetryType: "dismiss",
          telemetryId: this.notification.id,
          slot: "actions",
          dataL10nId:
            "contextual-manager-passwords-update-password-success-button",
          onClick: this.onDismiss,
        },
        notificationId: this.notification.id,
      })}
    `;
  }

  #renderDeleteLoginSuccess() {
    return html`
      ${notificationShell({
        onDismiss: this.#handleDismiss,
        dataL10nId:
          "contextual-manager-passwords-delete-password-success-heading",
        dataL10nArgs: JSON.stringify(this.notification.l10nArgs),
        dataL10nAttrs: "heading",
        type: "success",
        primaryAction: {
          type: "primary",
          telemetryType: "dismiss",
          telemetryId: this.notification.id,
          slot: "actions",
          dataL10nId:
            "contextual-manager-passwords-delete-password-success-button",
          onClick: this.onDismiss,
        },
        notificationId: this.notification.id,
      })}
    `;
  }

  #renderDiscardChanges() {
    return html`${notificationShell({
      onDismiss: this.#handleDismiss,
      dataL10nId:
        "contextual-manager-passwords-discard-changes-heading-and-message",
      type: "warning",
      primaryAction: {
        type: "destructive",
        telemetryType: "confirm_discard_changes",
        telemetryId: this.notification.id,
        dataL10nId: "contextual-manager-passwords-discard-changes-close-button",
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
        telemetryType: "dismiss",
        telemetryId: this.notification.id,
        dataL10nId:
          "contextual-manager-passwords-discard-changes-go-back-button",
        onClick: this.onDismiss,
      },
      notificationId: this.notification.id,
    })}`;
  }

  #renderBreachedOriginWarning() {
    return html`
      ${notificationShell({
        dismissable: false,
        messageHandler: this.messageHandler,
        dataL10nId:
          "contextual-manager-passwords-breached-origin-heading-and-message",
        type: "error",
        link: {
          url: this.notification.url,
          dataL10nId:
            "contextual-manager-passwords-breached-origin-link-message",
        },
        primaryAction: {
          telemetryType: "change_record",
          telemetryId: this.notification.id,
          slot: "actions",
          dataL10nId: "contextual-manager-passwords-change-password-button",
          onClick: this.notification.onButtonClick,
        },
        notificationId: this.notification.id,
      })}
    `;
  }

  #renderNoUsernameWarning() {
    return html`
      ${notificationShell({
        dismissable: false,
        dataL10nId:
          "contextual-manager-passwords-no-username-heading-and-message",
        type: "info",
        primaryAction: {
          telemetryType: "change_record",
          telemetryId: this.notification.id,
          slot: "actions",
          dataL10nId: "contextual-manager-passwords-add-username-button",
          onClick: this.notification.onButtonClick,
        },
        notificationId: this.notification.id,
      })}
    `;
  }

  #renderVulnerablePasswordWarning() {
    return html`
      ${notificationShell({
        dismissable: false,
        messageHandler: this.messageHandler,
        dataL10nId:
          "contextual-manager-passwords-vulnerable-password-heading-and-message",
        type: "warning",
        link: {
          url: this.notification.url,
          dataL10nId:
            "contextual-manager-passwords-vulnerable-password-link-message",
        },
        primaryAction: {
          telemetryType: "change_record",
          telemetryId: this.notification.id,
          slot: "actions",
          dataL10nId: "contextual-manager-passwords-change-password-button",
          onClick: this.notification.onButtonClick,
        },
        notificationId: this.notification.id,
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
