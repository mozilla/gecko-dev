/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, classMap } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/megalist/PasswordCard.mjs";

const DISPLAY_MODES = {
  ALERTS: "alerts",
  ALL: "all",
};

export class MegalistAlpha extends MozLitElement {
  constructor() {
    super();
    this.selectedIndex = 0;
    this.searchText = "";
    this.records = [];
    this.header = null;
    this.displayMode = DISPLAY_MODES.ALL;

    window.addEventListener("MessageFromViewModel", ev =>
      this.#onMessageFromViewModel(ev)
    );
  }

  static get properties() {
    return {
      selectedIndex: { type: Number },
      searchText: { type: String },
      records: { type: Array },
      header: { type: Object },
      displayMode: { type: String },
    };
  }

  connectedCallback() {
    super.connectedCallback();
    this.#messageToViewModel("Refresh");
  }

  #onMessageFromViewModel({ detail }) {
    const functionName = `receive${detail.name}`;
    if (!(functionName in this)) {
      console.warn(`Received unknown message "${detail.name}"`);
    }
    this[functionName]?.(detail.data);
  }

  #onInputChange(e) {
    const searchText = e.target.value;
    this.searchText = searchText;
    this.#messageToViewModel("UpdateFilter", { searchText });
  }

  #onAddButtonClick() {
    // TODO: implement me!
  }

  #onShowAllButtonClick() {
    this.displayMode = DISPLAY_MODES.ALL;
    this.#sendCommand("SortByName");
  }

  #openMenu(e) {
    const panelList = this.shadowRoot.querySelector("panel-list");
    panelList.toggle(e);
  }

  #onSortByAlertsButtonClick() {
    this.displayMode = DISPLAY_MODES.ALERTS;
    this.#sendCommand("SortByAlerts");
  }

  #messageToViewModel(messageName, data) {
    window.windowGlobalChild
      .getActor("Megalist")
      .sendAsyncMessage(messageName, data);
  }

  #sendCommand(commandId, options = {}) {
    // TODO(Bug 1913302): snapshotId should be optional for global commands.
    // Right now, we always pass 0 and overwrite when needed.
    this.#messageToViewModel("Command", {
      commandId,
      snapshotId: 0,
      ...options,
    });
  }

  receiveShowSnapshots({ snapshots }) {
    const [header, records] = this.#createLoginRecords(snapshots);
    this.header = header;
    this.records = records;
  }

  receiveSnapshot({ snapshotId, snapshot }) {
    const recordIndex = Math.floor((snapshotId - 1) / 3);
    const field = snapshot.field;
    this.records[recordIndex][field] = snapshot;
    this.requestUpdate();
  }

  #createLoginRecords(snapshots) {
    const header = snapshots.shift();
    const records = [];

    for (let i = 0; i < snapshots.length; i += 3) {
      records.push({
        origin: snapshots[i],
        username: snapshots[i + 1],
        password: snapshots[i + 2],
      });
    }

    return [header, records];
  }

  // TODO: This should be passed to virtualized list with an explicit height.
  renderListItem({ origin: displayOrigin, username, password }) {
    return html`<password-card
      .origin=${displayOrigin}
      .username=${username}
      .password=${password}
      .messageToViewModel=${this.#messageToViewModel.bind(this)}
    ></password-card>`;
  }

  // TODO: Temporary. Should be rendered by the virtualized list.
  renderList() {
    return this.records.length
      ? html`
          <div class="passwords-list">
            ${this.records.map(record => this.renderListItem(record))}
          </div>
        `
      : "";
  }

  renderSearch() {
    return html`
      <div class="searchContainer">
        <div class="searchIcon"></div>
        <input
          class="search"
          type="search"
          data-l10n-id="filter-input"
          .value=${this.searchText}
          @input=${e => this.#onInputChange(e)}
        />
      </div>
    `;
  }

  renderFirstRow() {
    return html`<div class="first-row">
      ${this.renderSearch()}
      <moz-button
        @click=${this.#onAddButtonClick}
        data-l10n-id="create-login-button"
        type="icon"
        iconSrc="chrome://global/skin/icons/plus.svg"
      ></moz-button>
    </div>`;
  }

  renderButton(title, selected, onClick) {
    return html`<moz-button
      class=${classMap({ selected })}
      iconSrc=${selected ? "chrome://global/skin/icons/check.svg" : ""}
      @click=${onClick}
    >
      ${title}
    </moz-button>`;
  }

  renderToggleButtons() {
    return html`
      <div class="toggle-buttons">
        ${this.renderButton(
          `All (${this.header.value.total})`,
          this.displayMode === DISPLAY_MODES.ALL,
          this.#onShowAllButtonClick
        )}
        ${this.renderButton(
          `Alerts (${this.header.value.alerts})`,
          this.displayMode === DISPLAY_MODES.ALERTS,
          this.#onSortByAlertsButtonClick
        )}
      </div>
    `;
  }

  renderMenu() {
    return html`
      <moz-button
        @click=${this.#openMenu}
        type="icon ghost"
        iconSrc="chrome://global/skin/icons/more.svg"
        aria-expanded="false"
        aria-haspopup="menu"
        data-l10n-id="menu-more-options-button"
        id="more-options-menubutton"
      ></moz-button>
      <panel-list
        role="menu"
        aria-labelledby="more-options-menubutton"
        data-l10n-id="more-options-popup"
      >
        <panel-item
          data-l10n-id="about-logins-menu-menuitem-import-from-another-browser"
        ></panel-item>
        <panel-item
          data-l10n-id="about-logins-menu-menuitem-import-from-a-file"
        ></panel-item>
        <panel-item
          data-l10n-id="about-logins-menu-menuitem-export-logins2"
        ></panel-item>
        <panel-item
          data-l10n-id="about-logins-menu-menuitem-remove-all-logins2"
        ></panel-item>
        <hr />
        <panel-item
          data-l10n-id="menu-menuitem-preferences"
          @click=${() => this.#sendCommand("Settings")}
        ></panel-item>
        <panel-item
          data-l10n-id="about-logins-menu-menuitem-help"
          @click=${() => this.#sendCommand("Help")}
        ></panel-item>
      </panel-list>
    `;
  }

  renderSecondRow() {
    if (!this.header) {
      return "";
    }

    return html`<div class="second-row">
      ${this.renderToggleButtons()} ${this.renderMenu()}
    </div>`;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/megalist/megalist.css"
      />
      <div class="container">
        ${this.renderFirstRow()} ${this.renderSecondRow()} ${this.renderList()}
      </div>
    `;
  }
}

customElements.define("megalist-alpha", MegalistAlpha);
