/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/* eslint-disable-next-line import/no-unassigned-import */
import "chrome://global/content/megalist/LoginLine.mjs";

const DIRECTIONS = {
  ArrowUp: -1,
  ArrowLeft: -1,
  ArrowDown: 1,
  ArrowRight: 1,
};

export class PasswordCard extends MozLitElement {
  static properties = {
    origin: { type: Object },
    username: { type: Object },
    password: { type: Object },
    messageToViewModel: { type: Function },
    reauthCommandHandler: { type: Function },
    onPasswordRevealClick: { type: Function },
    handleEditButtonClick: { type: Function },
    handleViewAlertClick: { type: Function },
  };

  static get queries() {
    return {
      originLine: ".line-item[linetype='origin']",
      usernameLine: ".line-item[linetype='username']",
      passwordLine: "concealed-login-line",
      editBtn: ".edit-button",
    };
  }

  #focusableElementsList;
  #focusableElementsMap;

  /**
   * Returns the first focusable element of the next password card componenet.
   * If the user is navigating down, then the next focusable element should be the edit button,
   * and if the user is navigating up, then it should be the origin line.
   *
   * @param {string} keyCode - The code associated with a keypress event. Either 'ArrowUp' or 'ArrowDown'.
   * @returns {HTMLElement | null} The first focusable element of the next password-card.
   */
  #getNextFocusableElement(keyCode) {
    return keyCode === "ArrowDown"
      ? this.nextElementSibling?.originLine
      : this.previousElementSibling?.editBtn;
  }

  async firstUpdated() {
    this.#focusableElementsMap = new Map();
    const buttons = this.shadowRoot.querySelectorAll("moz-button");
    const lineItems = this.shadowRoot.querySelectorAll(".line-item");

    let index = 0;
    for (const el of lineItems) {
      if (el === this.passwordLine) {
        await el.updateComplete;
        this.#focusableElementsMap.set(el.loginLine, index++);
        this.#focusableElementsMap.set(el.revealBtn.buttonEl, index++);
      } else {
        this.#focusableElementsMap.set(el, index++);
      }
    }

    for (const el of buttons) {
      this.#focusableElementsMap.set(el.buttonEl, index);
      index++;
    }

    this.#focusableElementsList = Array.from(this.#focusableElementsMap.keys());
  }

  #handleKeydown(e) {
    const element = e.composedTarget;

    const focusInternal = offset => {
      const index = this.#focusableElementsMap.get(element);
      this.#focusableElementsList[index + offset].focus();
    };

    switch (e.code) {
      case "ArrowUp":
        e.preventDefault();
        if (this.#focusableElementsMap.get(element) === 0) {
          this.#getNextFocusableElement(e.code)?.focus();
        } else {
          focusInternal(DIRECTIONS[e.code]);
        }
        break;
      case "ArrowDown":
        e.preventDefault();
        if (
          this.#focusableElementsMap.get(element) ===
          this.#focusableElementsList.length - 1
        ) {
          this.#getNextFocusableElement(e.code)?.focus();
        } else {
          focusInternal(DIRECTIONS[e.code]);
        }
        break;
    }
  }

  connectedCallback() {
    super.connectedCallback();
    this.addEventListener("keydown", e => this.#handleKeydown(e), {
      capture: true,
    });
  }

  handleCommand(commandId, lineIndex) {
    this.messageToViewModel("Command", { commandId, snapshotId: lineIndex });
  }

  async onEditButtonClick() {
    const isAuthenticated = await this.reauthCommandHandler(() =>
      this.messageToViewModel("Command", {
        commandId: "Edit",
        snapshotId: this.password.lineIndex,
      })
    );

    if (!isAuthenticated) {
      return;
    }

    this.handleEditButtonClick();
  }

  onViewAlertClick() {
    this.handleViewAlertClick();
  }

  #onOriginLineClick(lineIndex) {
    this.handleCommand("OpenLink", lineIndex);
  }

  #onCopyButtonClick(lineIndex) {
    this.handleCommand("Copy", lineIndex);
  }

  renderOriginField() {
    return html`
      <login-line
        tabindex="-1"
        role="option"
        class="line-item"
        data-l10n-id="origin-login-line"
        data-l10n-args="${JSON.stringify({ url: this.origin.value })}"
        inputType="text"
        lineType="origin"
        labelL10nId="passwords-origin-label"
        .value=${this.origin.value}
        .favIcon=${this.origin.valueIcon}
        ?alert=${this.origin.breached}
        .onLineClick=${() => {
          this.#onOriginLineClick(this.origin.lineIndex);
          return true;
        }}
      >
      </login-line>
    `;
  }

  renderUsernameField() {
    return html`
      <login-line
        tabindex="-1"
        role="option"
        class="line-item"
        data-l10n-id="username-login-line"
        data-l10n-args="${JSON.stringify({ username: this.username.value })}"
        inputType="text"
        lineType="username"
        labelL10nId="passwords-username-label"
        .value=${this.username.value}
        .onLineClick=${() => {
          this.#onCopyButtonClick(this.username.lineIndex);
          return true;
        }}
        ?alert=${!this.username.value.length}
      >
      </login-line>
    `;
  }

  renderPasswordField() {
    return html`
      <concealed-login-line
        class="line-item"
        labelL10nId="passwords-password-label"
        .value=${this.password.value}
        .visible=${!this.password.concealed}
        ?alert=${this.password.vulnerable}
        .onLineClick=${() =>
          this.reauthCommandHandler(() =>
            this.#onCopyButtonClick(this.password.lineIndex)
          )}
        .onButtonClick=${() =>
          this.reauthCommandHandler(() =>
            this.onPasswordRevealClick(
              this.password.concealed,
              this.password.lineIndex
            )
          )}
      >
      </concealed-login-line>
    `;
  }

  renderButton() {
    return html`<div class="edit-line-container" role="option">
      <moz-button
        data-l10n-id="edit-login-button"
        class="edit-button"
        @click=${this.onEditButtonClick}
      ></moz-button>
    </div>`;
  }

  renderViewAlertField() {
    const hasAlert =
      this.origin.breached ||
      !this.username.value.length ||
      this.password.vulnerable;

    if (!hasAlert) {
      return "";
    }

    return html`
      <moz-message-bar type="warning" data-l10n-id="view-alert-heading">
        <moz-button
          class="view-alert-button"
          data-l10n-id="view-alert-button"
          slot="actions"
          type="icon"
          iconSrc="chrome://browser/skin/forward.svg"
          @click=${this.onViewAlertClick}
        >
        </moz-button>
      </moz-message-bar>
    `;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/megalist/PasswordCard.css"
      />
      ${this.renderOriginField()} ${this.renderUsernameField()}
      ${this.renderPasswordField()} ${this.renderButton()}
      ${this.renderViewAlertField()}
    `;
  }
}

customElements.define("password-card", PasswordCard);
