/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * The widget for enabling password protection if the backup is not yet
 * encrypted.
 */
export default class PasswordRulesTooltip extends MozLitElement {
  static properties = {
    hasCommon: { type: Boolean },
    hasEmail: { type: Boolean },
    tooShort: { type: Boolean },
    supportBaseLink: { type: String },
  };

  static get queries() {
    return {
      passwordRulesEl: "#password-rules-wrapper",
    };
  }

  constructor() {
    super();
    this.hasCommon = false;
    this.hasEmail = false;
    this.tooShort = false;
    this.supportBaseLink = "";
  }

  getRuleStateConstants(hasInvalidCondition) {
    if (hasInvalidCondition) {
      return {
        class: "warning",
        icon: "chrome://global/skin/icons/warning.svg",
        l10nId: "password-rules-a11y-warning",
      };
    }

    return {
      class: "success",
      icon: "chrome://global/skin/icons/check-filled.svg",
      l10nId: "password-rules-a11y-success",
    };
  }

  render() {
    let lengthConstants = this.getRuleStateConstants(this.tooShort);
    let emailConstants = this.getRuleStateConstants(this.hasEmail);
    // TODO: (bug 1905140) read list of common passwords - default to success state for now
    let commonConstants = this.getRuleStateConstants(this.hasCommon);

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/backup/password-rules-tooltip.css"
      />
      <div id="password-rules-wrapper" aria-live="polite">
        <h2
          id="password-rules-header"
          data-l10n-id="password-rules-header"
        ></h2>
        <ul>
          <li>
            <img
              data-l10n-id=${lengthConstants.l10nId}
              class="icon ${lengthConstants.class}"
              src=${lengthConstants.icon}
            />
            <span
              data-l10n-id="password-rules-length-description"
              class="rule-description"
            ></span>
          </li>
          <li>
            <img
              data-l10n-id=${emailConstants.l10nId}
              class="icon ${emailConstants.class}"
              src=${emailConstants.icon}
            />
            <span
              data-l10n-id="password-rules-email-description"
              class="rule-description"
            ></span>
          </li>
          <li>
            <img
              data-l10n-id=${commonConstants.l10nId}
              class="icon ${commonConstants.class}"
              src=${commonConstants.icon}
            />
            <span
              data-l10n-id="password-rules-common-description"
              class="rule-description"
            ></span>
          </li>
          <li>
            <img
              class="icon"
              src="chrome://browser/skin/preferences/category-privacy-security.svg"
            />
            <span data-l10n-id="password-rules-disclaimer"
              ><a
                data-l10n-name="password-support-link"
                target="_blank"
                href=${`${this.supportBaseLink}password-strength`}
              ></a
            ></span>
          </li>
        </ul>
      </div>
    `;
  }
}

customElements.define("password-rules-tooltip", PasswordRulesTooltip);
