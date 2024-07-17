/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/* eslint-disable-next-line import/no-unassigned-import, mozilla/no-browser-refs-in-toolkit */
import "chrome://browser/content/aboutlogins/components/input-field/login-origin-field.mjs";
/* eslint-disable-next-line import/no-unassigned-import, mozilla/no-browser-refs-in-toolkit */
import "chrome://browser/content/aboutlogins/components/input-field/login-username-field.mjs";
/* eslint-disable-next-line import/no-unassigned-import, mozilla/no-browser-refs-in-toolkit */
import "chrome://browser/content/aboutlogins/components/input-field/login-password-field.mjs";

export class PasswordCard extends MozLitElement {
  static properties = {
    origin: { type: Object },
    username: { type: Object },
    password: { type: Object },
    messageToViewModel: { type: Function },
  };

  #revealIconSrc(concealed) {
    return !concealed
      ? /* eslint-disable-next-line mozilla/no-browser-refs-in-toolkit */
        "chrome://browser/content/aboutlogins/icons/password-hide.svg"
      : /* eslint-disable-next-line mozilla/no-browser-refs-in-toolkit */
        "chrome://browser/content/aboutlogins/icons/password.svg";
  }

  handleCommand(commandId, lineIndex) {
    this.messageToViewModel("Command", { commandId, snapshotId: lineIndex });
  }

  onEditButtonClick() {
    // TODO: Implement me!
  }

  onCopyButtonClick(lineIndex) {
    this.handleCommand("Copy", lineIndex);
  }

  onPasswordRevealClick(concealed, lineIndex) {
    if (concealed) {
      this.handleCommand("Reveal", lineIndex);
    } else {
      this.handleCommand("Conceal", lineIndex);
    }
  }

  renderOriginField() {
    return html`<login-origin-field readonly .value=${this.origin.value}>
      <moz-button
        slot="actions"
        @click=${() => this.onCopyButtonClick(this.origin.lineIndex)}
        type="icon ghost"
        iconSrc="chrome://global/skin/icons/edit-copy.svg"
      ></moz-button>
    </login-origin-field>`;
  }

  renderUsernameField() {
    return html`<login-username-field readonly .value=${this.username.value}>
      <moz-button
        slot="actions"
        @click=${() => this.onCopyButtonClick(this.username.lineIndex)}
        @click=${this.onCopyButtonClick}
        type="icon ghost"
        iconSrc="chrome://global/skin/icons/edit-copy.svg"
      ></moz-button>
    </login-username-field>`;
  }

  renderPasswordField() {
    return html`<login-password-field
      readonly
      .value=${this.password.value}
      .visible=${!this.password.concealed}
    >
      <moz-button
        slot="actions"
        @click=${() =>
          this.onPasswordRevealClick(
            this.password.concealed,
            this.password.lineIndex
          )}
        type="icon ghost"
        iconSrc=${this.#revealIconSrc(this.password.concealed)}
      ></moz-button>
      <moz-button
        slot="actions"
        @click=${() => this.onCopyButtonClick(this.password.lineIndex)}
        type="icon ghost"
        iconSrc="chrome://global/skin/icons/edit-copy.svg"
      ></moz-button>
    </login-password-field>`;
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
