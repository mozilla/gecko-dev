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

const LOGIN_FIELDS_LENGTH = 3;

export class PasswordCard extends MozLitElement {
  static properties = {
    origin: { type: Object },
    username: { type: Object },
    password: { type: Object },
    messageToViewModel: { type: Function },
  };

  static shadowRootOptions = {
    ...MozLitElement.shadowRootOptions,
    delegatesFocus: true,
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
   * @returns {HTMLDivElement | MozButton | null} The first focusable element of the next password-card.
   */
  #getNextFocusableElement(keyCode) {
    const cardIndex = Math.floor(this.origin.lineIndex / LOGIN_FIELDS_LENGTH);
    const passwordCards = this.parentNode.querySelectorAll("password-card");
    const nextCardIndex = cardIndex + DIRECTIONS[keyCode];

    if (nextCardIndex < 0 || nextCardIndex >= passwordCards.length) {
      return null;
    }

    const nextPasswordCard = passwordCards[nextCardIndex];
    return keyCode === "ArrowDown"
      ? nextPasswordCard.originLine.lineContainer
      : nextPasswordCard.editBtn;
  }

  async firstUpdated() {
    this.#focusableElementsMap = new Map();

    let index = 0;
    for (const el of this.shadowRoot.querySelectorAll(".line-item")) {
      await el.updateComplete;
      if (el === this.passwordLine) {
        this.#focusableElementsMap.set(el.loginLine.lineContainer, index++);
        this.#focusableElementsMap.set(el.revealBtn.buttonEl, index++);
      } else {
        this.#focusableElementsMap.set(el.lineContainer, index++);
      }
    }

    this.#focusableElementsMap.set(this.editBtn.buttonEl, index);
    this.#focusableElementsList = Array.from(this.#focusableElementsMap.keys());
  }

  #handleKeydown(element, e) {
    const targetIsPasswordLine =
      this.passwordLine.loginLine.lineContainer === element;
    const targetIsRevealButton =
      this.passwordLine.revealBtn.buttonEl === element;

    const focusInternal = offset => {
      const index = this.#focusableElementsMap.get(element);
      this.#focusableElementsList[index + offset].focus();
    };

    switch (e.code) {
      case "ArrowUp":
        if (this.#focusableElementsMap.get(element) === 0) {
          const nextFocusableElement = this.#getNextFocusableElement(e.code);
          nextFocusableElement?.focus();
        } else if (targetIsRevealButton) {
          focusInternal(-2);
        } else {
          focusInternal(DIRECTIONS[e.code]);
        }
        break;
      case "ArrowDown":
        if (
          this.#focusableElementsMap.get(element) ===
          this.#focusableElementsList.length - 1
        ) {
          const nextFocusableElement = this.#getNextFocusableElement(e.code);
          nextFocusableElement?.focus();
        } else if (targetIsPasswordLine) {
          focusInternal(2);
        } else {
          focusInternal(DIRECTIONS[e.code]);
        }
        break;
      case "ArrowRight":
        if (targetIsPasswordLine) {
          focusInternal(DIRECTIONS[e.code]);
        }
        break;
      case "ArrowLeft":
        if (targetIsRevealButton) {
          focusInternal(DIRECTIONS[e.code]);
        }
        break;
      default:
        return;
    }
    e.stopPropagation();
  }

  connectedCallback() {
    super.connectedCallback();
    this.addEventListener(
      "keydown",
      e => {
        const element = e.composedTarget;
        this.#handleKeydown(element, e);
      },
      { capture: true }
    );
  }

  handleCommand(commandId, lineIndex) {
    this.messageToViewModel("Command", { commandId, snapshotId: lineIndex });
  }

  onEditButtonClick() {
    // TODO: Implement me!
  }

  #onOriginLineClick(lineIndex) {
    this.handleCommand("OpenLink", lineIndex);
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
    return html`
      <login-line
        role="option"
        tabIndex="0"
        class="line-item"
        data-l10n-id="origin-login-line"
        data-l10n-args="${JSON.stringify({ url: this.origin.value })}"
        inputType="text"
        lineType="origin"
        labelL10nId="passwords-origin-label"
        .value=${this.origin.value}
        .favIcon=${this.origin.valueIcon}
        ?alert=${this.origin.breached}
        @click=${() => this.#onOriginLineClick(this.origin.lineIndex)}
        @keypress=${e => {
          if (e.key === "Enter") {
            this.#onOriginLineClick(this.origin.lineIndex);
          }
        }}
      >
      </login-line>
    `;
  }

  renderUsernameField() {
    return html`
      <login-line
        tabIndex="0"
        role="option"
        class="line-item"
        data-l10n-id="username-login-line"
        data-l10n-args="${JSON.stringify({ username: this.username.value })}"
        inputType="text"
        lineType="username"
        labelL10nId="passwords-username-label"
        .value=${this.username.value}
        ?alert=${this.username.value.length === 0}
        @click=${() => this.onCopyButtonClick(this.username.lineIndex)}
        @keypress=${e => {
          if (e.key === "Enter") {
            this.onCopyButtonClick(this.username.lineIndex);
          }
        }}
      >
      </login-line>
    `;
  }

  renderPasswordField() {
    return html`
      <concealed-login-line
        role="option"
        class="line-item"
        labelL10nId="passwords-password-label"
        .value=${this.password.value}
        .visible=${!this.password.concealed}
        ?alert=${this.password.vulnerable}
        .onLineClick=${() => this.onCopyButtonClick(this.password.lineIndex)}
        }}
        .onButtonClick=${() =>
          this.onPasswordRevealClick(
            this.password.concealed,
            this.password.lineIndex
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
        @mousedown=${e => e.preventDefault()}
        @click=${this.onEditButtonClick}
      ></moz-button>
    </div>`;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/megalist/PasswordCard.css"
      />
      ${this.renderOriginField()} ${this.renderUsernameField()}
      ${this.renderPasswordField()} ${this.renderButton()}
    `;
  }
}

customElements.define("password-card", PasswordCard);
