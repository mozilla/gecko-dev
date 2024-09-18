/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  when,
  ifDefined,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

class LoginLine extends MozLitElement {
  static shadowRootOptions = {
    ...MozLitElement.shadowRootOptions,
    delegatesFocus: true,
  };

  static properties = {
    value: { type: String },
    labelL10nId: { type: String },
    lineType: { type: String },
    inputType: { type: String },
    favIcon: { type: String },
    alert: { type: Boolean },
  };

  #copyTimeoutID;

  static get queries() {
    return {
      lineContainer: ".line-container",
    };
  }

  #canCopy() {
    return this.lineType !== "origin";
  }

  #addCopyAttr() {
    return ifDefined(this.#canCopy() ? "data-after" : undefined);
  }

  #handleLineClick() {
    if (!this.#canCopy()) {
      return;
    }
    if (!this.lineContainer.classList.contains("copied")) {
      this.lineContainer.classList.add("copied");
      this.#copyTimeoutID = setTimeout(() => {
        this.lineContainer.classList.remove("copied");
        this.#copyTimeoutID = null;
      }, 4000);
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    if (this.#copyTimeoutID) {
      clearTimeout(this.#copyTimeoutID);
    }
  }

  constructor() {
    super();
    this.favIcon = "";
    this.#copyTimeoutID = null;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/megalist/LoginLine.css"
      />
      <div
        class="line-container"
        tabindex="0"
        @click=${() => this.#handleLineClick()}
        @keypress=${e => {
          if (e.code == "Enter") {
            this.#handleLineClick();
          }
        }}
      >
        <div class="input-container">
          <div class="label-container">
            <label
              data-l10n-id=${this.labelL10nId}
              data-l10n-attrs=${this.#addCopyAttr()}
              class="input-label"
              for="login-line-input"
            ></label>
            ${when(
              this.alert,
              () =>
                html` <img
                  data-l10n-id="alert-icon"
                  class="alert-icon"
                  src="chrome://global/skin/icons/warning-fill-12.svg"
                />`
            )}
          </div>
          <div class="value-container">
            ${when(
              this.favIcon,
              () =>
                html` <img
                  data-l10n-id="website-icon"
                  class="fav-icon"
                  src=${this.favIcon}
                />`
            )}
            <input
              class="input-field"
              id="login-line-input"
              value=${this.value}
              type=${this.inputType}
              readonly
            />
          </div>
        </div>
        ${when(this.#canCopy(), () => {
          return html`
            <div class="copy-container">
              <img
                data-l10n-id="copy-icon"
                class="copy-icon"
                src="chrome://global/skin/icons/edit-copy.svg"
              />
              <img
                data-l10n-id="check-icon"
                class="check-icon"
                src="chrome://global/skin/icons/check.svg"
              />
            </div>
          `;
        })}
      </div>
    `;
  }
}

class ConcealedLoginLine extends MozLitElement {
  static properties = {
    value: { type: String },
    labelL10nId: { type: String },
    alert: { type: Boolean },
    visible: { type: Boolean },
    onLineClick: { type: Function },
    onButtonClick: { type: Function },
  };

  static CONCEALED_VALUE_TEXT = " ".repeat(8);

  static get queries() {
    return {
      loginLine: "login-line",
      revealBtn: "moz-button",
    };
  }

  get #inputType() {
    return !this.visible ? "password" : "text";
  }

  get #displayValue() {
    return !this.visible ? ConcealedLoginLine.CONCEALED_VALUE_TEXT : this.value;
  }

  get #revealBtnLabel() {
    return !this.visible ? "show-password-button" : "hide-password-button";
  }

  #revealIconSrc() {
    return this.visible
      ? /* eslint-disable-next-line mozilla/no-browser-refs-in-toolkit */
        "chrome://browser/content/aboutlogins/icons/password-hide.svg"
      : /* eslint-disable-next-line mozilla/no-browser-refs-in-toolkit */
        "chrome://browser/content/aboutlogins/icons/password.svg";
  }

  render() {
    return html` <link
        rel="stylesheet"
        href="chrome://global/content/megalist/LoginLine.css"
      />
      <login-line
        tabIndex="0"
        data-l10n-id="password-login-line"
        lineType="password"
        inputType=${this.#inputType}
        labelL10nId=${this.labelL10nId}
        .value=${this.#displayValue}
        ?alert=${this.alert}
        @click=${this.onLineClick}
        @keypress=${e => {
          if (e.key === "Enter") {
            this.onLineClick();
          }
        }}
      >
      </login-line>
      <div class="reveal-button-container">
        <moz-button
          class="reveal-button"
          type="icon ghost"
          data-l10n-id=${this.#revealBtnLabel}
          iconSrc=${this.#revealIconSrc()}
          @mousedown=${e => e.preventDefault()}
          @keypress=${e => {
            if (e.key === "Enter") {
              this.revealBtn.setAttribute("data-l10n-id", this.#revealBtnLabel);
              this.loginLine.focus();
              this.onButtonClick();
            }
          }}
          @click=${() => {
            this.revealBtn.setAttribute("data-l10n-id", this.#revealBtnLabel);
            this.onButtonClick();
          }}
        ></moz-button>
      </div>`;
  }
}

customElements.define("login-line", LoginLine);
customElements.define("concealed-login-line", ConcealedLoginLine);
