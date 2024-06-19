/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/backup/turn-on-scheduled-backups.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/backup/turn-off-scheduled-backups.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/backup/restore-from-backup.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/backup/disable-backup-encryption.mjs";

/**
 * The widget for managing the BackupService that is embedded within the main
 * document of about:settings / about:preferences.
 */
export default class BackupSettings extends MozLitElement {
  static properties = {
    backupServiceState: { type: Object },
  };

  static get queries() {
    return {
      scheduledBackupsButtonEl: "#backup-toggle-scheduled-button",
      disableBackupEncryptionEl: "disable-backup-encryption",
      disableBackupEncryptionDialogEl: "#disable-backup-encryption-dialog",
      turnOnScheduledBackupsDialogEl: "#turn-on-scheduled-backups-dialog",
      turnOnScheduledBackupsEl: "turn-on-scheduled-backups",
      turnOffScheduledBackupsEl: "turn-off-scheduled-backups",
      turnOffScheduledBackupsDialogEl: "#turn-off-scheduled-backups-dialog",
      restoreFromBackupEl: "restore-from-backup",
      restoreFromBackupButtonEl: "#backup-toggle-restore-button",
      restoreFromBackupDialogEl: "#restore-from-backup-dialog",
      sensitiveDataCheckboxInputEl: "#backup-sensitive-data-checkbox-input",
    };
  }

  /**
   * Creates a BackupPreferences instance and sets the initial default
   * state.
   */
  constructor() {
    super();
    this.backupServiceState = {
      backupDirPath: "",
      backupFileToRestore: null,
      backupFileInfo: null,
      backupInProgress: false,
      defaultParent: {
        fileName: "",
        path: "",
        iconURL: "",
      },
      encryptionEnabled: false,
      scheduledBackupsEnabled: false,
    };
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

    this.addEventListener("turnOnScheduledBackups", this);
    this.addEventListener("turnOffScheduledBackups", this);
    this.addEventListener("dialogCancel", this);
    this.addEventListener("getBackupFileInfo", this);
    this.addEventListener("disableEncryption", this);
    this.addEventListener("restoreFromBackupConfirm", this);
    this.addEventListener("restoreFromBackupChooseFile", this);
  }

  handleEvent(event) {
    switch (event.type) {
      case "turnOnScheduledBackups":
        this.turnOnScheduledBackupsDialogEl.close();
        this.dispatchEvent(
          new CustomEvent("BackupUI:ToggleScheduledBackups", {
            bubbles: true,
            composed: true,
            detail: {
              ...event.detail,
              isScheduledBackupsEnabled: true,
            },
          })
        );
        break;
      case "turnOffScheduledBackups":
        this.turnOffScheduledBackupsDialogEl.close();
        this.dispatchEvent(
          new CustomEvent("BackupUI:ToggleScheduledBackups", {
            bubbles: true,
            composed: true,
            detail: {
              isScheduledBackupsEnabled: false,
            },
          })
        );
        break;
      case "dialogCancel":
        if (this.turnOnScheduledBackupsDialogEl.open) {
          this.turnOnScheduledBackupsDialogEl.close();
        } else if (this.turnOffScheduledBackupsDialogEl.open) {
          this.turnOffScheduledBackupsDialogEl.close();
        } else if (this.restoreFromBackupDialogEl.open) {
          this.restoreFromBackupDialogEl.close();
        } else if (this.disableBackupEncryptionDialogEl.open) {
          this.disableBackupEncryptionDialogEl.close();
        }
        break;
      case "restoreFromBackupConfirm":
        this.restoreFromBackupDialogEl.close();
        this.dispatchEvent(
          new CustomEvent("BackupUI:RestoreFromBackupFile", {
            bubbles: true,
            composed: true,
            detail: {
              backupFile: event.detail.backupFile,
            },
          })
        );
        break;
      case "restoreFromBackupChooseFile":
        this.dispatchEvent(
          new CustomEvent("BackupUI:RestoreFromBackupChooseFile", {
            bubbles: true,
            composed: true,
          })
        );
        break;
      case "getBackupFileInfo":
        this.dispatchEvent(
          new CustomEvent("BackupUI:GetBackupFileInfo", {
            bubbles: true,
            composed: true,
            detail: {
              backupFile: event.detail.backupFile,
              backupPassword: event.detail.backupPassword,
            },
          })
        );
        break;
      case "disableEncryption":
        this.disableBackupEncryptionDialogEl.close();
        this.dispatchEvent(
          new CustomEvent("BackupUI:ToggleEncryption", {
            bubbles: true,
            composed: true,
            detail: {
              isEncryptionEnabled: false,
            },
          })
        );
        break;
    }
  }

  handleShowScheduledBackups() {
    if (
      !this.backupServiceState.scheduledBackupsEnabled &&
      this.turnOnScheduledBackupsDialogEl
    ) {
      this.turnOnScheduledBackupsDialogEl.showModal();
    } else if (
      this.backupServiceState.scheduledBackupsEnabled &&
      this.turnOffScheduledBackupsDialogEl
    ) {
      this.turnOffScheduledBackupsDialogEl.showModal();
    }
  }

  handleToggleBackupEncryption(event) {
    event.preventDefault();

    // Checkbox was unchecked, meaning encryption is already enabled and should be disabled.
    let toggledToDisable =
      !event.target.checked && this.backupServiceState.encryptionEnabled;

    if (toggledToDisable && this.disableBackupEncryptionDialogEl) {
      this.disableBackupEncryptionDialogEl.showModal();
    }
    // TODO: else, show enable encryption dialog (bug 1893295)
  }

  turnOnScheduledBackupsDialogTemplate() {
    let { fileName, path, iconURL } = this.backupServiceState.defaultParent;
    return html`<dialog id="turn-on-scheduled-backups-dialog">
      <turn-on-scheduled-backups
        defaultlabel=${fileName}
        defaultpath=${path}
        defaulticonurl=${iconURL}
      ></turn-on-scheduled-backups>
    </dialog>`;
  }

  turnOffScheduledBackupsDialogTemplate() {
    return html`<dialog id="turn-off-scheduled-backups-dialog">
      <turn-off-scheduled-backups></turn-off-scheduled-backups>
    </dialog>`;
  }

  restoreFromBackupDialogTemplate() {
    let { backupFilePath, backupFileToRestore, backupFileInfo } =
      this.backupServiceState;
    return html`<dialog id="restore-from-backup-dialog">
      <restore-from-backup
        .backupFilePath=${backupFilePath}
        .backupFileToRestore=${backupFileToRestore}
        .backupFileInfo=${backupFileInfo}
      ></restore-from-backup>
    </dialog>`;
  }

  restoreFromBackupTemplate() {
    return html`<div id="restore-from-backup">
      ${this.restoreFromBackupDialogTemplate()}

      <moz-button
        id="backup-toggle-restore-button"
        @click=${this.handleShowRestoreDialog}
        data-l10n-id="settings-data-backup-restore-choose"
      ></moz-button>
    </div>`;
  }

  handleShowRestoreDialog() {
    if (this.restoreFromBackupDialogEl) {
      this.restoreFromBackupDialogEl.showModal();
    }
  }

  disableBackupEncryptionDialogTemplate() {
    return html`<dialog id="disable-backup-encryption-dialog">
      <disable-backup-encryption></disable-backup-encryption>
    </dialog>`;
  }

  render() {
    return html`<link
        rel="stylesheet"
        href="chrome://browser/skin/preferences/preferences.css"
      />
      <link
        rel="stylesheet"
        href="chrome://browser/content/backup/backup-settings.css"
      />
      <div id="scheduled-backups">
        <div>
          Backup in progress:
          ${this.backupServiceState.backupInProgress ? "Yes" : "No"}
        </div>

        ${this.turnOnScheduledBackupsDialogTemplate()}
        ${this.turnOffScheduledBackupsDialogTemplate()}
        ${this.disableBackupEncryptionDialogTemplate()}

        <moz-button
          id="backup-toggle-scheduled-button"
          @click=${this.handleShowScheduledBackups}
          data-l10n-id="settings-data-backup-toggle"
        ></moz-button>

        ${this.restoreFromBackupTemplate()}

        <!-- TODO: we can use the moz-checkbox reusable component once it is ready (bug 1901635)-->
        <div id="backup-sensitive-data-checkbox">
          <label
            id="backup-sensitive-data-checkbox-label"
            for="backup-sensitive-data-checkbox-input"
          >
            <input
              id="backup-sensitive-data-checkbox-input"
              @click=${this.handleToggleBackupEncryption}
              type="checkbox"
              .checked=${this.backupServiceState.encryptionEnabled}
            />
            <span
              id="backup-sensitive-data-checkbox-span"
              data-l10n-id="settings-data-toggle-encryption-label"
            ></span>
          </label>
          <div
            id="backup-sensitive-data-checkbox-description"
            class="text-deemphasized"
          >
            <span
              id="backup-sensitive-data-checkbox-description-span"
              data-l10n-id="settings-data-toggle-encryption-description"
            ></span>
            <!--TODO: finalize support page links (bug 1900467)-->
            <a
              id="settings-data-toggle-encryption-learn-more-link"
              is="moz-support-link"
              support-page="todo-backup"
              data-l10n-id="settings-data-toggle-encryption-support-link"
            ></a>
          </div>
        </div>
      </div>`;
  }
}

customElements.define("backup-settings", BackupSettings);
