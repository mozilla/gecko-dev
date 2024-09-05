/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

import { html, when } from "chrome://global/content/vendor/lit.all.mjs";
import { navigateToLink } from "chrome://browser/content/firefoxview/helpers.mjs";

import { SidebarPage } from "./sidebar-page.mjs";

ChromeUtils.defineESModuleGetters(lazy, {
  HistoryController: "resource:///modules/HistoryController.sys.mjs",
  Sanitizer: "resource:///modules/Sanitizer.sys.mjs",
});

const NEVER_REMEMBER_HISTORY_PREF = "browser.privatebrowsing.autostart";
const DAYS_EXPANDED_INITIALLY = 2;

export class SidebarHistory extends SidebarPage {
  static queries = {
    cards: { all: "moz-card" },
    emptyState: "fxview-empty-state",
    lists: { all: "sidebar-tab-list" },
    menuButton: ".menu-button",
    searchTextbox: "fxview-search-textbox",
  };

  controller = new lazy.HistoryController(this, {
    component: "sidebar",
  });

  connectedCallback() {
    super.connectedCallback();
    const { document: doc } = this.topWindow;
    this._menu = doc.getElementById("sidebar-history-menu");
    this._menuSortByDate = doc.getElementById("sidebar-history-sort-by-date");
    this._menuSortBySite = doc.getElementById("sidebar-history-sort-by-site");
    this._menu.addEventListener("command", this);
    this.addContextMenuListeners();
    this.addSidebarFocusedListeners();
    this.controller.updateCache();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this._menu.removeEventListener("command", this);
    this.removeContextMenuListeners();
    this.removeSidebarFocusedListeners();
  }

  handleContextMenuEvent(e) {
    this.triggerNode = this.findTriggerNode(e, "sidebar-tab-row");
    if (!this.triggerNode) {
      e.preventDefault();
    }
  }

  handleCommandEvent(e) {
    switch (e.target.id) {
      case "sidebar-history-sort-by-date":
        this.controller.onChangeSortOption(e, "date");
        break;
      case "sidebar-history-sort-by-site":
        this.controller.onChangeSortOption(e, "site");
        break;
      case "sidebar-history-clear":
        lazy.Sanitizer.showUI(this.topWindow);
        break;
      case "sidebar-history-context-delete-page":
        this.controller.deleteFromHistory();
        break;
      default:
        super.handleCommandEvent(e);
        break;
    }
  }

  handleSidebarFocusedEvent() {
    this.searchTextbox?.focus();
  }

  onPrimaryAction(e) {
    navigateToLink(e);
  }

  onSecondaryAction(e) {
    this.triggerNode = e.detail.item;
    this.controller.deleteFromHistory();
  }

  /**
   * The template to use for cards-container.
   */
  get cardsTemplate() {
    if (this.controller.searchResults) {
      return this.#searchResultsTemplate();
    } else if (!this.controller.isHistoryEmpty) {
      return this.#historyCardsTemplate();
    }
    return this.#emptyMessageTemplate();
  }

  #historyCardsTemplate() {
    const { historyVisits } = this.controller;
    switch (this.controller.sortOption) {
      case "date":
        return historyVisits.map(
          ({ l10nId, items }, i) =>
            html` <moz-card
              type="accordion"
              ?expanded=${i < DAYS_EXPANDED_INITIALLY}
              data-l10n-attrs="heading"
              data-l10n-id=${l10nId}
              data-l10n-args=${JSON.stringify({
                date: items[0].time,
              })}
            >
              <div>${this.#tabListTemplate(this.getTabItems(items))}</div>
            </moz-card>`
        );
      case "site":
        return historyVisits.map(
          ({ domain, items }) =>
            html` <moz-card type="accordion" expanded heading=${domain}>
              <div>${this.#tabListTemplate(this.getTabItems(items))}</div>
            </moz-card>`
        );
      default:
        return [];
    }
  }

  #emptyMessageTemplate() {
    let descriptionHeader;
    let descriptionLabels;
    let descriptionLink;
    if (Services.prefs.getBoolPref(NEVER_REMEMBER_HISTORY_PREF, false)) {
      // History pref set to never remember history
      descriptionHeader = "firefoxview-dont-remember-history-empty-header";
      descriptionLabels = [
        "firefoxview-dont-remember-history-empty-description",
        "firefoxview-dont-remember-history-empty-description-two",
      ];
      descriptionLink = {
        url: "about:preferences#privacy",
        name: "history-settings-url-two",
      };
    } else {
      descriptionHeader = "firefoxview-history-empty-header";
      descriptionLabels = [
        "firefoxview-history-empty-description",
        "firefoxview-history-empty-description-two",
      ];
      descriptionLink = {
        url: "about:preferences#privacy",
        name: "history-settings-url",
      };
    }
    return html`
      <fxview-empty-state
        headerLabel=${descriptionHeader}
        .descriptionLabels=${descriptionLabels}
        .descriptionLink=${descriptionLink}
        class="empty-state history"
        isSelectedTab
        mainImageUrl="chrome://browser/content/firefoxview/history-empty.svg"
        openLinkInParentWindow
      >
      </fxview-empty-state>
    `;
  }

  #searchResultsTemplate() {
    return html` <moz-card
      data-l10n-attrs="heading"
      data-l10n-id="sidebar-search-results-header"
      data-l10n-args=${JSON.stringify({
        query: this.controller.searchQuery,
      })}
    >
      <div>
        ${when(
          this.controller.searchResults.length,
          () =>
            html`<h3
              slot="secondary-header"
              data-l10n-id="firefoxview-search-results-count"
              data-l10n-args="${JSON.stringify({
                count: this.controller.searchResults.length,
              })}"
            ></h3>`
        )}
        ${this.#tabListTemplate(
          this.getTabItems(this.controller.searchResults),
          this.controller.searchQuery
        )}
      </div>
    </moz-card>`;
  }

  #tabListTemplate(tabItems, searchQuery) {
    return html`<sidebar-tab-list
      maxTabsLength="-1"
      .searchQuery=${searchQuery}
      secondaryActionClass="delete-button"
      .tabItems=${tabItems}
      @fxview-tab-list-primary-action=${this.onPrimaryAction}
      @fxview-tab-list-secondary-action=${this.onSecondaryAction}
    >
    </sidebar-tab-list>`;
  }

  onSearchQuery(e) {
    this.controller.onSearchQuery(e);
  }

  getTabItems(items) {
    return items.map(item => ({
      ...item,
      secondaryL10nId: "sidebar-history-delete",
      secondaryL10nArgs: null,
    }));
  }

  openMenu(e) {
    const menuPos = this.sidebarController._positionStart
      ? "after_start" // Sidebar is on the left. Open menu to the right.
      : "after_end"; // Sidebar is on the right. Open menu to the left.
    this._menu.openPopup(e.target, menuPos, 0, 0, false, false, e);
  }

  shouldUpdate() {
    // don't update/render until initial history visits entries are available
    return !this.controller.isHistoryPending;
  }

  willUpdate() {
    this._menuSortByDate.setAttribute(
      "checked",
      this.controller.sortOption == "date"
    );
    this._menuSortBySite.setAttribute(
      "checked",
      this.controller.sortOption == "site"
    );
  }

  render() {
    return html`
      ${this.stylesheet()}
      <link
        rel="stylesheet"
        href="chrome://browser/content/sidebar/sidebar-history.css"
      />
      <div class="sidebar-panel">
        <sidebar-panel-header
          data-l10n-id="sidebar-menu-history-header"
          data-l10n-attrs="heading"
          view="viewHistorySidebar"
        >
        </sidebar-panel-header>
        <div class="options-container">
          <fxview-search-textbox
            data-l10n-id="firefoxview-search-text-box-history"
            data-l10n-attrs="placeholder"
            @fxview-search-textbox-query=${this.onSearchQuery}
            .size=${15}
          ></fxview-search-textbox>
          <moz-button
            class="menu-button"
            @click=${this.openMenu}
            view=${this.view}
            size="small"
            type="icon ghost"
          >
          </moz-button>
        </div>
        ${this.cardsTemplate}
      </div>
    `;
  }
}

customElements.define("sidebar-history", SidebarHistory);
