/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * The widget for allowing users to select and restore from a
 * a backup file.
 */
export default class RestoreFromBackup extends MozLitElement {
  #placeholderFileIconURL = "chrome://global/skin/icons/page-portrait.svg";

  static properties = {
    backupFilePath: { type: String },
    backupFileToRestore: { type: String, reflect: true },
    backupFileInfo: { type: Object },
    _fileIconURL: { type: String },
  };

  static get queries() {
    return {
      filePicker: "#backup-filepicker-input",
      passwordInput: "#backup-password-input",
      cancelButtonEl: "#restore-from-backup-cancel-button",
      confirmButtonEl: "#restore-from-backup-confirm-button",
      chooseButtonEl: "#backup-filepicker-button",
    };
  }

  constructor() {
    super();
    this._fileIconURL = "";
  }

  /**
   * Dispatches the BackupUI:InitWidget custom event upon being attached to the
   * DOM, which registers with BackupUIChild for BackupService state updates.
   */
  connectedCallback() {
    super.connectedCallback();
    this.dispatchEvent(
      new CustomEvent("BackupUI:InitWidget", { bubbles: true })
    );

    if (this.backupFileToRestore && !this.backupFileInfo) {
      this.getBackupFileInfo();
    }

    this.addEventListener("BackupUI:SelectNewFilepickerPath", this);
  }

  handleEvent(event) {
    if (event.type == "BackupUI:SelectNewFilepickerPath") {
      let { path, iconURL } = event.detail;
      this.backupFileToRestore = path;
      this._fileIconURL = iconURL;
    }
  }

  willUpdate(changedProperties) {
    if (changedProperties.has("backupFileToRestore")) {
      this.getBackupFileInfo();
    }
  }

  async handleChooseBackupFile() {
    this.dispatchEvent(
      new CustomEvent("BackupUI:ShowFilepicker", {
        bubbles: true,
        detail: {
          win: window.browsingContext,
          filter: "filterHTML",
          displayDirectoryPath: this.backupFileToRestore,
        },
      })
    );
  }

  getBackupFileInfo() {
    let backupFile = this.backupFileToRestore;
    if (!backupFile) {
      return;
    }
    this.dispatchEvent(
      new CustomEvent("getBackupFileInfo", {
        bubbles: true,
        composed: true,
        detail: {
          backupFile,
        },
      })
    );
  }

  handleCancel() {
    this.dispatchEvent(
      new CustomEvent("dialogCancel", {
        bubbles: true,
        composed: true,
      })
    );
  }

  handleConfirm() {
    let backupFile = this.backupFileToRestore;
    if (!backupFile) {
      return;
    }
    let backupPassword = this.passwordInput?.value;
    this.dispatchEvent(
      new CustomEvent("restoreFromBackupConfirm", {
        bubbles: true,
        composed: true,
        detail: {
          backupFile,
          backupPassword,
        },
      })
    );
  }

  controlsTemplate() {
    let iconURL =
      this.backupFileToRestore &&
      (this._fileIconURL || this.#placeholderFileIconURL);
    return html`
      <fieldset id="backup-restore-controls">
        <fieldset id="backup-filepicker-controls">
          <label
            id="backup-filepicker-label"
            for="backup-filepicker-input"
            data-l10n-id="restore-from-backup-filepicker-label"
          ></label>
          <div id="backup-filepicker">
            <input
              id="backup-filepicker-input"
              type="text"
              readonly
              value=${this.backupFileToRestore}
              style="background-image: url(${ifDefined(iconURL)})"
            />
            <moz-button
              id="backup-filepicker-button"
              @click=${this.handleChooseBackupFile}
              data-l10n-id="restore-from-backup-file-choose-button"
              aria-controls="backup-filepicker-input"
            ></moz-button>
          </div>
        </fieldset>

        <fieldset id="password-entry-controls">
          ${this.backupFileInfo?.isEncrypted
            ? this.passwordEntryTemplate()
            : null}
        </fieldset>
      </fieldset>
    `;
  }

  passwordEntryTemplate() {
    return html` <fieldset id="backup-password">
      <label id="backup-password-label" for="backup-password-input">
        <span
          id="backup-password-span"
          data-l10n-id="restore-from-backup-password-label"
        ></span>
        <input type="password" id="backup-password-input" />
      </label>
      <label
        id="backup-password-description"
        data-l10n-id="restore-from-backup-password-description"
      ></label>
    </fieldset>`;
  }

  contentTemplate() {
    return html`
      <div
        id="restore-from-backup-wrapper"
        aria-labelledby="restore-from-backup-header"
        aria-describedby="restore-from-backup-description"
      >
        <h1
          id="restore-from-backup-header"
          class="heading-medium"
          data-l10n-id="restore-from-backup-header"
        ></h1>
        <main id="restore-from-backup-content">
          ${this.backupFileInfo ? this.descriptionTemplate() : null}
          ${this.controlsTemplate()}
        </main>

        <moz-button-group id="restore-from-backup-button-group">
          <moz-button
            id="restore-from-backup-cancel-button"
            @click=${this.handleCancel}
            data-l10n-id="restore-from-backup-cancel-button"
          ></moz-button>
          <moz-button
            id="restore-from-backup-confirm-button"
            @click=${this.handleConfirm}
            type="primary"
            data-l10n-id="restore-from-backup-confirm-button"
          ></moz-button>
        </moz-button-group>
      </div>
    `;
  }

  descriptionTemplate() {
    return html`
      <div id="restore-from-backup-description">
        <span
          id="restore-from-backup-description-span"
          data-l10n-id="restore-from-backup-description-with-metadata"
          data-l10n-args=${JSON.stringify({
            date: new Date(this.backupFileInfo?.date).getTime(),
          })}
        ></span>
        <a
          id="restore-from-backup-learn-more-link"
          is="moz-support-link"
          support-page="todo-backup"
          data-l10n-id="restore-from-backup-support-link"
        ></a>
      </div>
    `;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/backup/restore-from-backup.css"
      />
      ${this.contentTemplate()}
    `;
  }
}

customElements.define("restore-from-backup", RestoreFromBackup);
