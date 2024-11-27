/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  // eslint-disable-next-line mozilla/no-browser-refs-in-toolkit
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
});

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/megalist/PasswordCard.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/megalist/LoginFormComponent/login-form.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/megalist/Dialog.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/megalist/NotificationMessageBar.mjs";

const DISPLAY_MODES = {
  ALERTS: "SortByAlerts",
  ALL: "SortByName",
};

const VIEW_MODES = {
  LIST: "List",
  ADD: "Add",
};

const INPUT_CHANGE_DELAY = 300;

export class MegalistAlpha extends MozLitElement {
  constructor() {
    super();
    this.selectedIndex = 0;
    this.searchText = "";
    this.records = [];
    this.header = null;
    this.notification = null;
    this.reauthResolver = null;
    this.displayMode = DISPLAY_MODES.ALL;
    this.inputChangeTimeout = null;
    this.viewMode = VIEW_MODES.LIST;

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
      notification: { type: Object },
      displayMode: { type: String },
      viewMode: { type: String },
    };
  }

  connectedCallback() {
    super.connectedCallback();
    this.#messageToViewModel("Refresh");
    this.#sendCommand(this.displayMode);
  }

  async getUpdateComplete() {
    await super.getUpdateComplete();
    const passwordCards = Array.from(
      this.shadowRoot.querySelectorAll("password-card")
    );
    await Promise.all(passwordCards.map(el => el.updateComplete));
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

    this.#debounce(
      () => this.#messageToViewModel("UpdateFilter", { searchText }),
      INPUT_CHANGE_DELAY
    )();
  }

  #onAddButtonClick() {
    this.viewMode = VIEW_MODES.ADD;
  }

  #onRadioButtonChange(e) {
    this.displayMode = e.target.value;
    this.#sendCommand(this.displayMode);
  }

  #openMenu(e) {
    const panelList = this.shadowRoot.querySelector("panel-list");
    panelList.toggle(e);
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

  receiveSetNotification(notification) {
    this.notification = notification;
    if (notification.id === "add-login-success") {
      this.viewMode = VIEW_MODES.LIST;
    }
  }

  receiveReauthResponse(isAuthorized) {
    this.reauthResolver?.(isAuthorized);
  }

  reauthCommandHandler(commandFn) {
    return new Promise((resolve, _reject) => {
      this.reauthResolver = resolve;
      commandFn();
    });
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

  #debounce(callback, delay) {
    return () => {
      clearTimeout(this.inputChangeTimeout);
      this.inputChangeTimeout = setTimeout(() => {
        callback();
      }, delay);
    };
  }

  // TODO: This should be passed to virtualized list with an explicit height.
  renderListItem({ origin: displayOrigin, username, password }) {
    return html` <password-card
      @keypress=${e => {
        if (e.shiftKey && e.key === "Tab") {
          e.preventDefault();
          this.shadowRoot.querySelector("#more-options-menubutton").focus();
        } else if (e.key === "Tab") {
          e.preventDefault();
          const webContent =
            lazy.BrowserWindowTracker.getTopWindow().gBrowser.selectedTab
              .linkedBrowser;
          webContent.focus();
        }
      }}
      role="group"
      aria-label=${displayOrigin.value}
      .origin=${displayOrigin}
      .username=${username}
      .password=${password}
      .messageToViewModel=${this.#messageToViewModel.bind(this)}
      .reauthCommandHandler=${commandFn => this.reauthCommandHandler(commandFn)}
    >
    </password-card>`;
  }

  // TODO: Temporary. Should be rendered by the virtualized list.
  renderList() {
    return this.records.length
      ? html`
          <div
            class="passwords-list"
            role="listbox"
            tabindex="0"
            data-l10n-id="passwords-list-label"
            @keypress=${e => {
              if (e.key === "ArrowDown") {
                e.preventDefault();
                this.shadowRoot
                  .querySelector("password-card")
                  .originLine.focus();
              }
            }}
          >
            ${this.records.map(record => this.renderListItem(record))}
          </div>
        `
      : this.renderEmptyState();
  }

  renderEmptyState() {
    if (this.header) {
      const { total, count } = this.header.value;
      if (!total) {
        return this.renderNoLoginsCard();
      } else if (!count) {
        return this.renderEmptySearchResults();
      }
    }

    return "";
  }

  renderNoLoginsCard() {
    return html`
      <moz-card class="empty-state-card">
        <div class="no-logins-card-content">
          <strong
            class="no-logins-card-heading"
            data-l10n-id="passwords-no-passwords-header"
          ></strong>
          <p data-l10n-id="passwords-no-passwords-message"></p>
          <p data-l10n-id="passwords-no-passwords-get-started-message"></p>
          <div class="no-logins-card-buttons">
            <moz-button
              data-l10n-id="passwords-command-import-from-browser"
              type="primary"
              @click=${() => this.#sendCommand("ImportFromBrowser")}
            ></moz-button>
            <moz-button
              data-l10n-id="passwords-command-import"
              @click=${() => this.#sendCommand("Import")}
            ></moz-button>
            <moz-button
              data-l10n-id="passwords-add-manually"
              @click=${this.#onAddButtonClick}
            ></moz-button>
          </div>
        </div>
      </moz-card>
    `;
  }

  renderEmptySearchResults() {
    return html` <moz-card
      class="empty-state-card"
      data-l10n-id="passwords-no-passwords-found-header"
    >
      <div
        class="empty-search-results"
        data-l10n-id="passwords-no-passwords-found-message"
      ></div>
    </moz-card>`;
  }

  renderLastRow() {
    switch (this.viewMode) {
      case VIEW_MODES.LIST:
        return this.renderList();
      case VIEW_MODES.ADD:
        return html` <login-form
          .onCancelClick=${() => (this.viewMode = VIEW_MODES.LIST)}
          .onSaveClick=${formData => {
            this.#sendCommand("AddLogin", { value: formData });
          }}
        >
        </login-form>`;
      default:
        return "";
    }
  }

  renderSearch() {
    return html`
      <div
        class="searchContainer"
        @click=${() => {
          this.shadowRoot.querySelector(".search").focus();
        }}
      >
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

  renderRadioButtons() {
    return html`
      <div data-l10n-id="passwords-radiogroup-label" role="radiogroup">
        <input
          @change=${this.#onRadioButtonChange}
          checked
          type="radio"
          id="allLogins"
          name="logins"
          value=${DISPLAY_MODES.ALL}
        />
        <label
          for="allLogins"
          data-l10n-id="passwords-radiobutton-all"
          data-l10n-args=${JSON.stringify({ total: this.header.value.total })}
        ></label>

        <input
          @change=${this.#onRadioButtonChange}
          type="radio"
          id="alerts"
          name="logins"
          value=${DISPLAY_MODES.ALERTS}
        />
        <label
          for="alerts"
          data-l10n-id="passwords-radiobutton-alerts"
          data-l10n-args=${JSON.stringify({ total: this.header.value.alerts })}
        ></label>
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
          action="import-from-browser"
          data-l10n-id="about-logins-menu-menuitem-import-from-another-browser"
          @click=${() => this.#sendCommand("ImportFromBrowser")}
        ></panel-item>
        <panel-item
          action="import-from-file"
          data-l10n-id="about-logins-menu-menuitem-import-from-a-file"
          @click=${() => this.#sendCommand("Import")}
        ></panel-item>
        <panel-item
          action="export-logins"
          data-l10n-id="about-logins-menu-menuitem-export-logins2"
          @click=${() => this.#sendCommand("Export")}
        ></panel-item>
        <panel-item
          action="remove-all-logins"
          data-l10n-id="about-logins-menu-menuitem-remove-all-logins2"
          @click=${() => this.#sendCommand("RemoveAll")}
          .disabled=${!this.header.value.total}
        ></panel-item>
        <hr />
        <panel-item
          action="open-preferences"
          data-l10n-id="menu-menuitem-preferences"
          @click=${() => this.#sendCommand("Settings")}
        ></panel-item>
        <panel-item
          action="open-help"
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
      ${this.renderRadioButtons()} ${this.renderMenu()}
    </div>`;
  }

  async #scrollPasswordCardIntoView(guid) {
    const matchingRecordIndex = this.records.findIndex(
      record => record.origin.guid === guid
    );
    this.viewMode = VIEW_MODES.LIST;
    await this.getUpdateComplete();
    const passwordCard =
      this.shadowRoot.querySelectorAll("password-card")[matchingRecordIndex];
    passwordCard.scrollIntoView({ block: "center" });
    passwordCard.originLine.focus();
  }

  renderNotification() {
    if (!this.notification) {
      return "";
    }

    return html`
      <notification-message-bar
        .notification=${this.notification}
        .onDismiss=${() => {
          this.notification = null;
        }}
        .messageHandler=${commandId => this.#sendCommand(commandId)}
        @view-login=${e => this.#scrollPasswordCardIntoView(e.detail.guid)}
      >
      </notification-message-bar>
    `;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/megalist/megalist.css"
      />
      <div class="container">
        ${this.renderFirstRow()} ${this.renderSecondRow()}
        ${this.renderNotification()} ${this.renderLastRow()}
      </div>
    `;
  }
}

customElements.define("megalist-alpha", MegalistAlpha);
