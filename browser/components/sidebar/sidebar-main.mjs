/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  ifDefined,
  nothing,
  repeat,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * Sidebar with expanded and collapsed states that provides entry points
 * to various sidebar panels and sidebar extensions.
 */
export default class SidebarMain extends MozLitElement {
  static properties = {
    bottomActions: { type: Array },
    expanded: { type: Boolean, reflect: true },
    selectedView: { type: String },
    sidebarItems: { type: Array },
    open: { type: Boolean },
  };

  static queries = {
    allButtons: { all: "moz-button" },
    extensionButtons: { all: ".tools-and-extensions > moz-button[extension]" },
    toolButtons: { all: ".tools-and-extensions > moz-button:not([extension])" },
    customizeButton: ".bottom-actions > moz-button[view=viewCustomizeSidebar]",
  };

  get fluentStrings() {
    if (!this._fluentStrings) {
      this._fluentStrings = new Localization(["browser/sidebar.ftl"], true);
    }
    return this._fluentStrings;
  }

  constructor() {
    super();
    this.bottomActions = [];
    this.selectedView = window.SidebarController.currentID;
    this.open = window.SidebarController.isOpen;
    this.contextMenuTarget = null;
    this.expanded = false;
  }

  connectedCallback() {
    super.connectedCallback();
    this._sidebarBox = document.getElementById("sidebar-box");
    this._sidebarMain = document.getElementById("sidebar-main");
    this._contextMenu = document.getElementById("sidebar-context-menu");
    this._manageExtensionMenuItem = document.getElementById(
      "sidebar-context-menu-manage-extension"
    );
    this._removeExtensionMenuItem = document.getElementById(
      "sidebar-context-menu-remove-extension"
    );
    this._reportExtensionMenuItem = document.getElementById(
      "sidebar-context-menu-report-extension"
    );

    this._sidebarBox.addEventListener("sidebar-show", this);
    this._sidebarBox.addEventListener("sidebar-hide", this);
    this._sidebarMain.addEventListener("contextmenu", this);
    this._contextMenu.addEventListener("popuphidden", this);
    this._contextMenu.addEventListener("command", this);

    window.addEventListener("SidebarItemAdded", this);
    window.addEventListener("SidebarItemChanged", this);
    window.addEventListener("SidebarItemRemoved", this);

    this.setCustomize();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this._sidebarBox.removeEventListener("sidebar-show", this);
    this._sidebarBox.removeEventListener("sidebar-hide", this);
    this._sidebarMain.removeEventListener("contextmenu", this);
    this._contextMenu.removeEventListener("popuphidden", this);
    this._contextMenu.removeEventListener("command", this);

    window.removeEventListener("SidebarItemAdded", this);
    window.removeEventListener("SidebarItemChanged", this);
    window.removeEventListener("SidebarItemRemoved", this);
  }

  onSidebarPopupShowing(event) {
    // Store the context menu target which holds the id required for managing sidebar items
    this.contextMenuTarget =
      event.explicitOriginalTarget.flattenedTreeParentNode;
    if (
      this.contextMenuTarget.getAttribute("extensionId") ||
      this.contextMenuTarget.className.includes("tab")
    ) {
      this.updateExtensionContextMenuItems();
      return;
    }
    event.preventDefault();
  }

  async updateExtensionContextMenuItems() {
    const extensionId = this.contextMenuTarget.getAttribute("extensionId");
    if (!extensionId) {
      return;
    }
    const addon = await window.AddonManager?.getAddonByID(extensionId);
    if (!addon) {
      // Disable all context menus items if the addon doesn't
      // exist anymore from the AddonManager perspective.
      this._manageExtensionMenuItem.disabled = true;
      this._removeExtensionMenuItem.disabled = true;
      this._reportExtensionMenuItem.disabled = true;
    } else {
      this._manageExtensionMenuItem.disabled = false;
      this._removeExtensionMenuItem.disabled = !(
        addon.permissions & AddonManager.PERM_CAN_UNINSTALL
      );
      this._reportExtensionMenuItem.disabled = !window.gAddonAbuseReportEnabled;
    }
  }

  async manageExtension() {
    await window.BrowserAddonUI.manageAddon(
      this.contextMenuTarget.getAttribute("extensionId"),
      "sidebar-context-menu"
    );
  }

  async removeExtension() {
    await window.BrowserAddonUI.removeAddon(
      this.contextMenuTarget.getAttribute("extensionId"),
      "sidebar-context-menu"
    );
  }

  async reportExtension() {
    await window.BrowserAddonUI.reportAddon(
      this.contextMenuTarget.getAttribute("extensionId"),
      "sidebar-context-menu"
    );
  }

  getImageUrl(icon, targetURI) {
    if (window.IS_STORYBOOK) {
      return `chrome://global/skin/icons/defaultFavicon.svg`;
    }
    if (!icon) {
      if (targetURI?.startsWith("moz-extension")) {
        return "chrome://mozapps/skin/extensions/extension.svg";
      }
      return `chrome://global/skin/icons/defaultFavicon.svg`;
    }
    // If the icon is not for website (doesn't begin with http), we
    // display it directly. Otherwise we go through the page-icon
    // protocol to try to get a cached version. We don't load
    // favicons directly.
    if (icon.startsWith("http")) {
      return `page-icon:${targetURI}`;
    }
    return icon;
  }

  getToolsAndExtensions() {
    return window.SidebarController.toolsAndExtensions;
  }

  setCustomize() {
    const view = "viewCustomizeSidebar";
    const customizeSidebar = window.SidebarController.sidebars.get(view);
    this.bottomActions = [
      {
        l10nId: customizeSidebar.revampL10nId,
        iconUrl: customizeSidebar.iconUrl,
        view,
      },
    ];
  }

  async handleEvent(e) {
    switch (e.type) {
      case "command":
        switch (e.target.id) {
          case "sidebar-context-menu-manage-extension":
            await this.manageExtension();
            break;
          case "sidebar-context-menu-report-extension":
            await this.reportExtension();
            break;
          case "sidebar-context-menu-remove-extension":
            await this.removeExtension();
            break;
        }
        break;
      case "contextmenu":
        this.onSidebarPopupShowing(e);
        break;
      case "popuphidden":
        this.contextMenuTarget = null;
        break;
      case "sidebar-show":
        this.selectedView = e.detail.viewId;
        this.open = true;
        break;
      case "sidebar-hide":
        this.open = false;
        break;
      case "SidebarItemAdded":
      case "SidebarItemChanged":
      case "SidebarItemRemoved":
        this.requestUpdate();
        break;
    }
  }

  showView(view) {
    window.SidebarController.toggle(view);
  }

  entrypointTemplate(action) {
    if (action.disabled || action.hidden) {
      return null;
    }
    const isActiveView = this.open && action.view === this.selectedView;
    let actionLabel = "";
    if (action.tooltiptext) {
      actionLabel = action.tooltiptext;
    } else if (action.l10nId) {
      const messages = this.fluentStrings.formatMessagesSync([action.l10nId]);
      const attributes = messages?.[0]?.attributes;
      actionLabel = attributes?.find(attr => attr.name === "label")?.value;
    }
    return html`<moz-button
      class=${this.expanded ? "expanded-button" : ""}
      type=${isActiveView ? "icon" : "icon ghost"}
      aria-pressed="${isActiveView}"
      view=${action.view}
      @click=${() => this.showView(action.view)}
      title=${!this.expanded ? actionLabel : nothing}
      .iconSrc=${action.iconUrl}
      ?extension=${action.view?.includes("-sidebar-action")}
      extensionId=${ifDefined(action.extensionId)}
    >
      ${this.expanded ? actionLabel : nothing}
    </moz-button>`;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/sidebar/sidebar.css"
      />
      <link
        rel="stylesheet"
        href="chrome://browser/content/sidebar/sidebar-main.css"
      />
      <div class="wrapper">
        <slot name="tabstrip"></slot>
        <button-group
          class="tools-and-extensions actions-list"
          orientation="vertical"
        >
          ${repeat(
            this.getToolsAndExtensions().values(),
            action => action.view,
            action => this.entrypointTemplate(action)
          )}
        </button-group>
        <div class="bottom-actions actions-list">
          ${repeat(
            this.bottomActions,
            action => action.view,
            action => this.entrypointTemplate(action)
          )}
        </div>
      </div>
    `;
  }
}

customElements.define("sidebar-main", SidebarMain);
