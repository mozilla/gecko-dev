/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export class PasswordCard extends MozLitElement {
  static properties = {
    origin: { type: Object },
    username: { type: Object },
    password: { type: Object },
    messageToViewModel: { type: Function },
  };

  // TODO: Pass to login field components.
  handleCommand(commandId, lineIndex) {
    this.messageToViewModel("Command", { commandId, snapshotId: lineIndex });
  }

  onEditButtonClick() {
    // TODO: Implement me!
  }

  renderOriginField() {
    // TODO: Implement me!
    return "";
  }

  renderUsernameField() {
    // TODO: Implement me!
    return "";
  }

  renderPasswordField() {
    // TODO: Implement me!
    return "";
  }

  renderButton() {
    return html`<moz-button
      data-l10n-id="login-item-edit-button"
      class="edit-button"
      @click=${this.onEditButtonClick}
    ></moz-button>`;
  }

  render() {
    return html` <link
        rel="stylesheet"
        href="chrome://global/content/megalist/megalist.css"
      />
      <moz-card>
        <div class="password-card-container">
          ${this.renderOriginField()} ${this.renderUsernameField()}
          ${this.renderPasswordField()} ${this.renderButton()}
        </div>
      </moz-card>`;
  }
}

customElements.define("password-card", PasswordCard);
