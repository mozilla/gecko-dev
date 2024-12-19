/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

export class SidebarPanelHeader extends MozLitElement {
  static properties = {
    view: { type: String },
    heading: { type: String },
  };

  static queries = {
    closeButton: ".header-close-button",
  };

  getWindow() {
    return window.browsingContext.embedderWindowGlobal.browsingContext.window;
  }

  closeSidebarPanel(e) {
    e.preventDefault();
    let view = e.target.getAttribute("view");
    this.getWindow().SidebarController.toggle(view);
  }

  render() {
    return html`
      <link rel="stylesheet" href="chrome://browser/content/sidebar/sidebar-panel-header.css"></link>
      <div class="sidebar-panel-header">
        <h4 class="text-truncated-ellipsis">${this.heading}</h4>
        <moz-button
          iconsrc="chrome://global/skin/icons/close.svg"
          data-l10n-id="sidebar-panel-header-close-button"
          @click=${this.closeSidebarPanel}
          view=${this.view}
          size="default"
          type="icon ghost"
        >
        </moz-button>
      </div>
    `;
  }
}
customElements.define("sidebar-panel-header", SidebarPanelHeader);
