/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

export default class IPProtectionContentElement extends MozLitElement {
  static queries = {
    upgradeEl: "#upgrade-vpn-content",
  };

  static properties = {
    state: { type: Object },
  };

  constructor() {
    super();

    this.state = {};
  }

  connectedCallback() {
    super.connectedCallback();
    this.dispatchEvent(new CustomEvent("IPProtection:Init", { bubbles: true }));
  }

  disconnectedCallback() {
    super.disconnectedCallback();
  }

  handleUpgrade() {
    // TODO: Handle click of Upgrade button - Bug 1975317
  }

  contentTemplate() {
    // TODO: Update support-page with new SUMO link for Mozilla VPN - Bug 1975474
    return html`
      <div id="upgrade-vpn-content">
        <h2 id="upgrade-vpn-title" data-l10n-id="upgrade-vpn-title"></h2>
        <p id="upgrade-vpn-paragraph" data-l10n-id="upgrade-vpn-paragraph">
          <a
            is="moz-support-link"
            data-l10n-name="learn-more-vpn"
            support-page="test"
          ></a>
        </p>
        <moz-button
          id="upgrade-vpn-button"
          @click=${this.handleUpgrade}
          type="secondary"
          data-l10n-id="upgrade-vpn-button"
        ></moz-button>
      </div>
    `;
  }

  render() {
    // TODO: Conditionally render subviews within #ipprotection-content-wrapper - Bug 1973813, Bug 1973815
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/ipprotection/ipprotection-content.css"
      />
      <div id="ipprotection-content-wrapper">${this.contentTemplate()}</div>
    `;
  }
}

customElements.define("ipprotection-content", IPProtectionContentElement);
