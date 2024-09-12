/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

const actionButton = action => {
  return html` <moz-button
    @click=${action.onClick}
    type=${ifDefined(action.type)}
    data-l10n-id=${ifDefined(action.l10nId)}
    data-l10n-args=${JSON.stringify(ifDefined(action.l10nArgs))}
    ?disabled=${action.disabled}
  ></moz-button>`;
};

const dialogShell = ({
  closeDialog,
  dialogContentTemplate,
  titlel10nId,
  titlel10nArgs,
  messageL10nId,
  messageL10nArgs,
  primaryAction,
  secondaryAction,
}) => {
  return html`
    <link
      rel="stylesheet"
      href="chrome://global/content/megalist/megalist.css"
    />
    <div class="dialog-overlay">
      <div class="dialog-container">
        <moz-button
          data-l10n-id="confirmation-dialog-dismiss-button"
          iconSrc="chrome://global/skin/icons/close.svg"
          size="small"
          type="icon ghost"
          class="dismiss-button"
          @click=${closeDialog}
        >
        </moz-button>
        <div class="dialog-wrapper">
          <h3
            class="dialog-title"
            data-l10n-id=${titlel10nId}
            data-l10n-args=${JSON.stringify(ifDefined(titlel10nArgs))}
          ></h3>
          <div class="dialog-content" slot="dialog-content">
            <p
              data-l10n-id=${messageL10nId}
              data-l10n-args=${ifDefined(messageL10nArgs)}
            ></p>
            ${dialogContentTemplate ?? ""}
          </div>
          <moz-button-group>
            ${actionButton(primaryAction)} ${actionButton(secondaryAction)}
          </moz-button-group>
        </div>
      </div>
    </div>
  `;
};

export class ExportAllDialog extends MozLitElement {
  render() {
    return html`
      ${dialogShell({
        closeDialog: this.onClose,
        titlel10nId: "about-logins-confirm-export-dialog-title2",
        messageL10nId: "about-logins-confirm-export-dialog-message2",
        primaryAction: {
          type: "primary",
          l10nId: "about-logins-confirm-export-dialog-confirm-button2",
          onClick: this.onClick,
        },
        secondaryAction: {
          l10nId: "confirmation-dialog-cancel-button",
          onClick: this.onClose,
        },
      })}
    `;
  }
}

class RemoveAllDialog extends MozLitElement {
  static properties = {
    enabled: { type: Boolean, state: true },
    loginsCount: { type: Number },
  };

  get l10nArgs() {
    return {
      count: this.loginsCount,
    };
  }

  contentTemplate() {
    return html` <label>
      <input
        type="checkbox"
        class="confirm-checkbox checkbox"
        @change=${() => (this.enabled = !this.enabled)}
        autofocus
      />
      <span
        class="checkbox-text"
        data-l10n-id="about-logins-confirm-remove-all-dialog-checkbox-label2"
        data-l10n-args=${JSON.stringify(this.l10nArgs)}
      ></span>
    </label>`;
  }

  render() {
    return html`
      ${dialogShell({
        closeDialog: this.onClose,
        dialogContentTemplate: this.contentTemplate(),
        titlel10nId: "about-logins-confirm-remove-all-sync-dialog-title2",
        titlel10nArgs: this.l10nArgs,
        messageL10nId: "about-logins-confirm-remove-all-dialog-message2",
        messageL10nArgs: this.l10nArgs,
        primaryAction: {
          type: "destructive",
          l10nId: "about-logins-confirm-remove-all-dialog-confirm-button-label",
          l10nArgs: this.l10nArgs,
          disabled: !this.enabled,
          onClick: this.onClick,
        },
        secondaryAction: {
          l10nId: "confirmation-dialog-cancel-button",
          onClick: this.onClose,
        },
      })}
    `;
  }
}

customElements.define("remove-all-dialog", RemoveAllDialog);
customElements.define("export-all-dialog", ExportAllDialog);
