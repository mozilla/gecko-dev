/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/megalist/PasswordCard.mjs";

export class MegalistAlpha extends MozLitElement {
  constructor() {
    super();
    this.selectedIndex = 0;
    this.searchText = "";
    this.records = [];
    this.header = null;

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

  #messageToViewModel(messageName, data) {
    window.windowGlobalChild
      .getActor("Megalist")
      .sendAsyncMessage(messageName, data);
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

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/megalist/megalist.css"
      />
      <div class="container">${this.renderList()}</div>
    `;
  }
}

customElements.define("megalist-alpha", MegalistAlpha);
