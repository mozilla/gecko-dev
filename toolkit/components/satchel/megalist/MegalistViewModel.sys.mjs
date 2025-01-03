/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DefaultAggregator } from "resource://gre/modules/megalist/aggregator/DefaultAggregator.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
});

/**
 *
 * Responsible for filtering lines data from the Aggregator and receiving user
 * commands from the view.
 *
 * Refers to the same MegalistAggregator in the parent process to access data.
 * Paired to exactly one MegalistView in the child process to present to the user.
 *
 */
export class MegalistViewModel {
  /**
   *
   * View Model prepares snapshots in the parent process to be displayed
   * by the View in the child process.
   *
   * View requests line data by providing snapshotId.
   *
   */
  #snapshots = [];
  #searchText = "";
  #messageToView;
  #authExpirationTime;

  static #aggregator = new DefaultAggregator();

  constructor(messageToView) {
    this.#messageToView = messageToView;
    this.#authExpirationTime = Number.NEGATIVE_INFINITY;
    MegalistViewModel.#aggregator.attachViewModel(this);
  }

  get authExpirationTime() {
    return this.#authExpirationTime;
  }

  willDestroy() {
    MegalistViewModel.#aggregator.detachViewModel(this);
  }

  refreshAllLinesOnScreen() {
    this.#rebuildSnapshots();
  }

  refreshSingleLineOnScreen(line) {
    const snapshotIndex = this.#snapshots.indexOf(line);
    if (snapshotIndex >= 0) {
      const snapshot = this.#processSnapshotView(line, snapshotIndex);
      this.#messageToView("Snapshot", {
        snapshotId: snapshotIndex,
        snapshot,
      });
    }
  }

  /**
   *
   * Send snapshot of necessary line data across parent-child boundary.
   *
   * @param {object} snapshotData
   * @param {number} index
   */
  #processSnapshotView(snapshotData, index) {
    const snapshot = {
      label: snapshotData.label,
      value: snapshotData.value,
      field: snapshotData.field,
      lineIndex: index,
    };

    if ("commands" in snapshotData) {
      snapshot.commands = snapshotData.commands;
    }

    if ("valueIcon" in snapshotData) {
      snapshot.valueIcon = snapshotData.valueIcon;
    }

    if ("href" in snapshotData) {
      snapshot.href = snapshotData.href;
    }

    if ("breached" in snapshotData) {
      snapshot.breached = snapshotData.breached;
    }

    if ("breachedNotification" in snapshotData) {
      snapshot.breachedNotification = snapshotData.breachedNotification;
    }

    if ("vulnerable" in snapshotData) {
      snapshot.vulnerable = snapshotData.vulnerable;
    }

    if ("vulnerableNotification" in snapshotData) {
      snapshot.vulnerableNotification = snapshotData.vulnerableNotification;
    }

    if ("noUsernameNotification" in snapshotData) {
      snapshot.noUsernameNotification = snapshotData.noUsernameNotification;
    }

    if ("toggleTooltip" in snapshotData) {
      snapshot.toggleTooltip = snapshotData.toggleTooltip;
    }

    if ("concealed" in snapshotData) {
      snapshot.concealed = snapshotData.concealed;
    }

    if ("record" in snapshotData) {
      snapshot.guid = snapshotData.record.guid;
    }

    return snapshot;
  }

  handleViewMessage({ name, data }) {
    const handlerName = `receive${name}`;
    if (!(handlerName in this)) {
      throw new Error(`Received unknown message "${name}"`);
    }
    return this[handlerName](data);
  }

  receiveRefresh() {
    this.#rebuildSnapshots();
  }

  #rebuildSnapshots() {
    this.#snapshots = Array.from(
      MegalistViewModel.#aggregator.enumerateLines(this.#searchText)
    );

    // Expose relevant line properties to view
    const viewSnapshots = this.#snapshots.map((snapshot, i) =>
      this.#processSnapshotView(snapshot, i)
    );

    // Update snapshots on screen
    this.#messageToView("ShowSnapshots", {
      snapshots: viewSnapshots,
    });
  }

  receiveUpdateFilter({ searchText } = { searchText: "" }) {
    if (this.#searchText != searchText) {
      this.#searchText = searchText;
      this.#rebuildSnapshots();
    }
  }

  setNotification(notification) {
    this.#messageToView("SetNotification", notification);
  }

  discardChangesConfirmed() {
    this.#messageToView("DiscardChangesConfirmed");
  }

  async receiveCommand({ commandId, snapshotId, value } = {}) {
    const dotIndex = commandId?.indexOf(".");
    const index = snapshotId;
    const snapshot = this.#snapshots[index];

    if (dotIndex >= 0) {
      const dataSourceName = commandId.substring(0, dotIndex);
      const functionName = commandId.substring(dotIndex + 1);
      MegalistViewModel.#aggregator.callFunction(
        dataSourceName,
        functionName,
        snapshot?.record
      );
      return;
    }

    if (snapshot) {
      const commands = snapshot.commands;
      commandId = commandId ?? commands[0]?.id;
      const command = snapshot.commands.find(c => c.id == commandId);
      if (!command?.verify || (await this.#promptForReauth(command))) {
        await snapshot[`execute${commandId}`]?.(value);
      }
    }
  }

  async #promptForReauth(command) {
    const { isAuthorized } = await lazy.LoginHelper.requestReauth(
      lazy.BrowserWindowTracker.getTopWindow().gBrowser,
      this.getOSAuthEnabled(),
      this.#authExpirationTime,
      command.OSAuthPromptMessage,
      command.OSAuthCaptionMessage
    );

    if (isAuthorized) {
      const authTimeoutMs = MegalistViewModel.#aggregator.callFunction(
        "LoginDataSource",
        "getAuthTimeoutMs"
      );
      this.#authExpirationTime = Date.now() + authTimeoutMs;
    }

    this.#messageToView("ReauthResponse", isAuthorized);
    return isAuthorized;
  }

  /**
   * Get the decrypted value for a string pref.
   *
   * @param {string} prefName -> The pref whose value is needed.
   * @param {string} safeDefaultValue -> Value to be returned incase the pref is not yet set.
   * @returns {string}
   */
  #getSecurePref(prefName, safeDefaultValue) {
    try {
      let encryptedValue = Services.prefs.getStringPref(prefName, "");
      return this._crypto.decrypt(encryptedValue);
    } catch {
      return safeDefaultValue;
    }
  }

  /**
   * Set the pref to the encrypted form of the value.
   *
   * @param {string} prefName -> The pref whose value is to be set.
   * @param {string} value -> The value to be set in its encryoted form.
   */
  #setSecurePref(prefName, value) {
    let encryptedValue = this._crypto.encrypt(value);
    Services.prefs.setStringPref(prefName, encryptedValue);
  }

  /**
   * Get whether the OSAuth is enabled or not.
   *
   * @param {string} prefName -> The name of the pref (creditcards or addresses)
   * @returns {boolean}
   */
  getOSAuthEnabled(prefName) {
    return this.#getSecurePref(prefName, "") !== "opt out";
  }

  /**
   * Set whether the OSAuth is enabled or not.
   *
   * @param {string} prefName -> The pref to encrypt.
   * @param {boolean} enable -> Whether the pref is to be enabled.
   */
  setOSAuthEnabled(prefName, enable) {
    if (enable) {
      Services.prefs.clearUserPref(prefName);
    } else {
      this.#setSecurePref(prefName, "opt out");
    }
  }
}
