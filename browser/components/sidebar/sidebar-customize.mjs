/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, when } from "chrome://global/content/vendor/lit.all.mjs";

import { SidebarPage } from "./sidebar-page.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-radio-group.mjs";

const l10nMap = new Map([
  ["viewGenaiChatSidebar", "sidebar-menu-genai-chat-label"],
  ["viewReviewCheckerSidebar", "sidebar-menu-review-checker-label"],
  ["viewHistorySidebar", "sidebar-menu-history-label"],
  ["viewTabsSidebar", "sidebar-menu-synced-tabs-label"],
  ["viewBookmarksSidebar", "sidebar-menu-bookmarks-label"],
]);
const VISIBILITY_SETTING_PREF = "sidebar.visibility";
const TAB_DIRECTION_SETTING_PREF = "sidebar.verticalTabs";

export class SidebarCustomize extends SidebarPage {
  constructor() {
    super();
    this.activeExtIndex = 0;
    this.visibility = Services.prefs.getStringPref(
      VISIBILITY_SETTING_PREF,
      "always-show"
    );
    this.boundObserve = (...args) => this.observe(...args);
  }

  static properties = {
    activeExtIndex: { type: Number },
    visibility: { type: String },
  };

  static queries = {
    toolInputs: { all: ".customize-group moz-checkbox" },
    extensionLinks: { all: ".extension-link" },
    positionInputs: { all: ".position-setting" },
    visibilityInputs: { all: ".visibility-setting" },
    verticalTabsInputs: { all: ".vertical-tabs-setting" },
  };

  connectedCallback() {
    super.connectedCallback();
    this.getWindow().addEventListener("SidebarItemAdded", this);
    this.getWindow().addEventListener("SidebarItemChanged", this);
    this.getWindow().addEventListener("SidebarItemRemoved", this);
    Services.prefs.addObserver(VISIBILITY_SETTING_PREF, this.boundObserve);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.getWindow().removeEventListener("SidebarItemAdded", this);
    this.getWindow().removeEventListener("SidebarItemChanged", this);
    this.getWindow().removeEventListener("SidebarItemRemoved", this);
    Services.prefs.removeObserver(VISIBILITY_SETTING_PREF, this.boundObserve);
  }

  observe(subject, topic, prefName) {
    switch (topic) {
      case "nsPref:changed":
        switch (prefName) {
          case VISIBILITY_SETTING_PREF:
            this.visibility = Services.prefs.getStringPref(
              VISIBILITY_SETTING_PREF,
              "always-show"
            );
            break;
        }
        break;
    }
  }

  get sidebarLauncher() {
    return this.getWindow().document.querySelector("sidebar-launcher");
  }

  getWindow() {
    return window.browsingContext.embedderWindowGlobal.browsingContext.window;
  }

  handleEvent(e) {
    switch (e.type) {
      case "SidebarItemAdded":
      case "SidebarItemChanged":
      case "SidebarItemRemoved":
        this.requestUpdate();
        break;
    }
  }

  async onToggleInput(e) {
    e.preventDefault();
    this.getWindow().SidebarController.toggleTool(e.target.id);
    switch (e.target.id) {
      case "viewGenaiChatSidebar":
        Glean.sidebarCustomize.chatbotEnabled.record({
          checked: e.target.checked,
        });
        break;
      case "viewTabsSidebar":
        Glean.sidebarCustomize.syncedTabsEnabled.record({
          checked: e.target.checked,
        });
        break;
      case "viewHistorySidebar":
        Glean.sidebarCustomize.historyEnabled.record({
          checked: e.target.checked,
        });
        break;
      case "viewBookmarksSidebar":
        Glean.sidebarCustomize.bookmarksEnabled.record({
          checked: e.target.checked,
        });
        break;
    }
  }

  getInputL10nId(view) {
    return l10nMap.get(view);
  }

  openFirefoxSettings(e) {
    if (e.type == "click" || (e.type == "keydown" && e.code == "Enter")) {
      e.preventDefault();
      this.getWindow().openPreferences();
      Glean.sidebarCustomize.firefoxSettingsClicked.record();
    }
  }

  inputTemplate(tool) {
    if (tool.hidden) {
      return null;
    }
    return html`
      <moz-checkbox
        type="checkbox"
        id=${tool.view}
        name=${tool.view}
        iconsrc=${tool.iconUrl}
        data-l10n-id=${this.getInputL10nId(tool.view)}
        @change=${this.onToggleInput}
        ?checked=${!tool.disabled}
      />
    `;
  }

  async manageAddon(extensionId) {
    await this.getWindow().BrowserAddonUI.manageAddon(
      extensionId,
      "unifiedExtensions"
    );
    Glean.sidebarCustomize.extensionsClicked.record();
  }

  handleKeydown(e) {
    if (e.code == "ArrowUp") {
      if (this.activeExtIndex > 0) {
        this.focusIndex(this.activeExtIndex - 1);
      }
    } else if (e.code == "ArrowDown") {
      if (this.activeExtIndex < this.extensionLinks.length - 1) {
        this.focusIndex(this.activeExtIndex + 1);
      }
    } else if (
      (e.type == "keydown" && e.code == "Enter") ||
      (e.type == "keydown" && e.code == "Space")
    ) {
      this.manageAddon(e.target.getAttribute("extensionId"));
    }
  }

  focusIndex(index) {
    let extLinkList = Array.from(
      this.shadowRoot.querySelectorAll(".extension-link")
    );
    extLinkList[index].focus();
    this.activeExtIndex = index;
  }

  reversePosition() {
    const { SidebarController } = this.getWindow();
    SidebarController.reversePosition();
    Glean.sidebarCustomize.sidebarPosition.record({
      position:
        SidebarController._positionStart !== this.getWindow().RTL_UI
          ? "left"
          : "right",
    });
  }

  extensionTemplate(extension, index) {
    return html` <div class="extension-item">
      <img src=${extension.iconUrl} class="icon" role="presentation" />
      <div
        extensionId=${extension.extensionId}
        role="listitem"
        @click=${() => this.manageAddon(extension.extensionId)}
        @keydown=${this.handleKeydown}
      >
        <a
          href="about:addons"
          class="extension-link"
          tabindex=${index === this.activeExtIndex ? 0 : -1}
          target="_blank"
          @click=${e => e.preventDefault()}
          >${extension.tooltiptext}
        </a>
      </div>
    </div>`;
  }

  render() {
    let extensions = this.getWindow().SidebarController.getExtensions();
    return html`
      ${this.stylesheet()}
      <link rel="stylesheet" href="chrome://browser/content/sidebar/sidebar-customize.css"></link>
      <div class="sidebar-panel">
        <sidebar-panel-header data-l10n-id="sidebar-menu-customize-header" data-l10n-attrs="heading" view="viewCustomizeSidebar">
        </sidebar-panel-header>
        <moz-fieldset class="customize-group" data-l10n-id="sidebar-customize-firefox-tools-header">
          ${this.getWindow()
            .SidebarController.getTools()
            .map(tool => this.inputTemplate(tool))}
        </moz-fieldset>
        ${when(
          extensions.length,
          () =>
            html`<div class="customize-group">
              <h4
                class="customize-extensions-heading"
                data-l10n-id="sidebar-customize-extensions-header"
              ></h4>
              <div role="list" class="extensions">
                ${extensions.map((extension, index) =>
                  this.extensionTemplate(extension, index)
                )}
              </div>
            </div>`
        )}
        <div class="customize-group">
          <moz-radio-group
            @change=${this.#handleVisibilityChange}
            name="visibility"
            data-l10n-id="sidebar-customize-button-header"
          >
            <moz-radio
              class="visibility-setting"
              value="always-show"
              ?checked=${this.visibility === "always-show"}
              iconsrc="chrome://browser/skin/sidebar-expanded.svg"
              data-l10n-id="sidebar-visibility-setting-always-show"
            ></moz-radio>
            <moz-radio
              class="visibility-setting"
              value="hide-sidebar"
              ?checked=${this.visibility === "hide-sidebar"}
            iconsrc="chrome://browser/skin/sidebar-hidden.svg"
              data-l10n-id="sidebar-visibility-setting-hide-sidebar"
            ></moz-radio>
          </moz-radio-group>
        </div>
        <div class="customize-group">
          <moz-radio-group
              @change=${this.reversePosition}
              name="position"
              data-l10n-id="sidebar-customize-position-header">
            <moz-radio
              class="position-setting"
              id="position-left"
              value=${!this.getWindow().RTL_UI}
              ?checked=${
                this.getWindow().RTL_UI
                  ? !this.getWindow().SidebarController._positionStart
                  : this.getWindow().SidebarController._positionStart
              }
              iconsrc="chrome://browser/skin/sidebar-expanded.svg"
              data-l10n-id="sidebar-position-left"
            ></moz-radio>
            <moz-radio
              class="position-setting"
              id="position-right"
              value=${this.getWindow().RTL_UI}
              ?checked=${
                this.getWindow().RTL_UI
                  ? this.getWindow().SidebarController._positionStart
                  : !this.getWindow().SidebarController._positionStart
              }
              iconsrc="chrome://browser/skin/sidebar-expanded-right.svg"
              data-l10n-id="sidebar-position-right"
            ></moz-radio>
          </moz-radio-group>
        </div>
        <div class="customize-group">
          <moz-radio-group
              @change=${this.#handleTabDirectionChange}
              name="tabDirection"
              data-l10n-id="sidebar-customize-tabs-header">
            <moz-radio
              class="vertical-tabs-setting"
              id="vertical-tabs"
              value=${true}
              ?checked=${
                this.getWindow().SidebarController.sidebarVerticalTabsEnabled
              }
              iconsrc="chrome://browser/skin/sidebar-collapsed.svg"
              data-l10n-id="sidebar-vertical-tabs"
            ></moz-radio>
            <moz-radio
              class="vertical-tabs-setting"
              id="horizontal-tabs"
              value=${false}
              ?checked=${
                this.getWindow().SidebarController
                  .sidebarVerticalTabsEnabled === false
              }
              iconsrc="chrome://browser/skin/sidebar-horizontal-tabs.svg"
              data-l10n-id="sidebar-horizontal-tabs"
            ></moz-radio>
          </moz-radio-group>
        </div>
        <div id="manage-settings">
          <img src="chrome://browser/skin/preferences/category-general.svg" class="icon" role="presentation" />
          <a
            href="about:preferences"
            @click=${this.openFirefoxSettings}
            @keydown=${this.openFirefoxSettings}
            data-l10n-id="sidebar-customize-firefox-settings"
          >
          </a>
        </div>
      </div>
    `;
  }

  #handleVisibilityChange({ target: { value } }) {
    this.visibility = value;
    Services.prefs.setStringPref(VISIBILITY_SETTING_PREF, value);
    Glean.sidebarCustomize.sidebarDisplay.record({
      preference: value === "always-show" ? "always" : "hide",
    });
  }

  #handleTabDirectionChange({ target: { value } }) {
    const verticalTabsEnabled = value === "true";
    Services.prefs.setBoolPref(TAB_DIRECTION_SETTING_PREF, verticalTabsEnabled);
    Glean.sidebarCustomize.tabsLayout.record({
      orientation: verticalTabsEnabled ? "vertical" : "horizontal",
    });
  }
}

customElements.define("sidebar-customize", SidebarCustomize);
