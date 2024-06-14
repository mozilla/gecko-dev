/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BackupService: "resource:///modules/backup/BackupService.sys.mjs",
});

/**
 * A JSWindowActor that is responsible for marshalling information between
 * the BackupService singleton and any registered UI widgets that need to
 * represent data from that service.
 */
export class BackupUIParent extends JSWindowActorParent {
  /**
   * A reference to the BackupService singleton instance.
   *
   * @type {BackupService}
   */
  #bs;

  /**
   * Create a BackupUIParent instance. If a BackupUIParent is instantiated
   * before BrowserGlue has a chance to initialize the BackupService, this
   * constructor will cause it to initialize first.
   */
  constructor() {
    super();
    // We use init() rather than get(), since it's possible to load
    // about:preferences before the service has had a chance to init itself
    // via BrowserGlue.
    this.#bs = lazy.BackupService.init();
  }

  /**
   * Called once the BackupUIParent/BackupUIChild pair have been connected.
   */
  actorCreated() {
    this.#bs.addEventListener("BackupService:StateUpdate", this);
  }

  /**
   * Called once the BackupUIParent/BackupUIChild pair have been disconnected.
   */
  didDestroy() {
    this.#bs.removeEventListener("BackupService:StateUpdate", this);
  }

  /**
   * Handles events fired by the BackupService.
   *
   * @param {Event} event
   *   The event that the BackupService emitted.
   */
  handleEvent(event) {
    if (event.type == "BackupService:StateUpdate") {
      this.sendState();
    }
  }

  /**
   * Handles messages sent by BackupUIChild.
   *
   * @param {ReceiveMessageArgument} message
   *   The message received from the BackupUIChild.
   */
  async receiveMessage(message) {
    if (message.name == "RequestState") {
      this.sendState();
    } else if (message.name == "ToggleScheduledBackups") {
      let { isScheduledBackupsEnabled, parentDirPath, password } = message.data;

      if (isScheduledBackupsEnabled && parentDirPath) {
        this.#bs.setParentDirPath(parentDirPath);
        /**
         * TODO: display an error and do not attempt to toggle scheduled backups if there
         * is a problem with setting the parent directory (bug 1901308).
         */
      }

      if (password) {
        try {
          await this.#bs.enableEncryption(password);
        } catch (e) {
          /**
           * TODO: display en error and do not attempt to toggle scheduled backups if there is a
           * problem with enabling encryption (bug 1901308)
           */
          return null;
        }
      }

      this.#bs.setScheduledBackups(isScheduledBackupsEnabled);

      return true;

      /**
       * TODO: (Bug 1900125) we should create a backup at the specified dir path once we turn on
       * scheduled backups. The backup folder in the chosen directory should contain
       * the archive file, which we create using BackupService.createArchive implemented in
       * Bug 1897498.
       */
    } else if (message.name == "ShowFilepicker") {
      let { win, filter, displayDirectoryPath } = message.data;

      let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);

      let mode = filter
        ? Ci.nsIFilePicker.modeOpen
        : Ci.nsIFilePicker.modeGetFolder;
      fp.init(win, "", mode);

      if (filter) {
        fp.appendFilters(Ci.nsIFilePicker[filter]);
      }

      if (displayDirectoryPath) {
        try {
          let exists = await IOUtils.exists(displayDirectoryPath);
          if (exists) {
            fp.displayDirectory = await IOUtils.getFile(displayDirectoryPath);
          }
        } catch (_) {
          // If the file can not be found we will skip setting the displayDirectory.
        }
      }

      let result = await new Promise(resolve => fp.open(resolve));

      if (result === Ci.nsIFilePicker.returnCancel) {
        return null;
      }

      let path = fp.file.path;
      let iconURL = this.#bs.getIconFromFilePath(path);
      let filename = PathUtils.filename(path);

      return {
        path,
        filename,
        iconURL,
      };
    } else if (message.name == "GetBackupFileInfo") {
      // TODO: Call sampleArchive method in BackupService
      // once it is implemented in Bug 1901132 and pass date and isEncrypted.
    } else if (message.name == "RestoreFromBackupChooseFile") {
      const window = this.browsingContext.topChromeWindow;
      this.#bs.filePickerForRestore(window);
    } else if (message.name == "RestoreFromBackupFile") {
      // TODO: Call restore from single-file archive method
      // in BackupService once it is implemented in Bug 1890322.
    }

    return null;
  }

  /**
   * Sends the StateUpdate message to the BackupUIChild, along with the most
   * recent state object from BackupService.
   */
  sendState() {
    this.sendAsyncMessage("StateUpdate", { state: this.#bs.state });
  }
}
