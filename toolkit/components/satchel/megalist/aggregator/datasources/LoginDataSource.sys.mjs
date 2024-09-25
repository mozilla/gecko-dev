/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { LoginHelper } from "resource://gre/modules/LoginHelper.sys.mjs";
import { DataSourceBase } from "resource://gre/modules/megalist/aggregator/datasources/DataSourceBase.sys.mjs";
import { LoginCSVImport } from "resource://gre/modules/LoginCSVImport.sys.mjs";
import { LoginExport } from "resource://gre/modules/LoginExport.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  LoginBreaches: "resource:///modules/LoginBreaches.sys.mjs",
  MigrationUtils: "resource:///modules/MigrationUtils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "BREACH_ALERTS_ENABLED",
  "signon.management.page.breach-alerts.enabled",
  false
);

// Precedence values when sorting logins by alerts.
const ALERT_VALUES = {
  breached: 0,
  vulnerable: 1,
  none: 2,
};

export const SUPPORT_URL =
  Services.urlFormatter.formatURLPref("app.support.baseURL") +
  "password-manager-remember-delete-edit-logins";

export const PREFERENCES_URL = "about:preferences#privacy-logins";

const IMPORT_FILE_SUPPORT_URL =
  "https://support.mozilla.org/kb/import-login-data-file";

const IMPORT_FILE_REPORT_URL = "about:loginsimportreport";

/**
 * Data source for Logins.
 *
 * Each login is represented by 3 lines: origin, username and password.
 *
 * Protypes are used to reduce memory need because for different records
 * similar lines will differ in values only.
 */
export class LoginDataSource extends DataSourceBase {
  doneReloadDataSource;
  #originPrototype;
  #usernamePrototype;
  #passwordPrototype;
  #enabled;
  #header;
  #exportPasswordsStrings;
  #sortId;

  constructor(...args) {
    super(...args);
    // Wait for Fluent to provide strings before loading data
    this.localizeStrings({
      headerLabel: "passwords-section-label",
      expandSection: "passwords-expand-section-tooltip",
      collapseSection: "passwords-collapse-section-tooltip",
      originLabel: "passwords-origin-label",
      usernameLabel: "passwords-username-label",
      passwordLabel: "passwords-password-label",
      passwordsDisabled: "passwords-disabled",
      passwordOSAuthDialogCaption: "passwords-os-auth-dialog-caption",
      passwordsImportFilePickerTitle: "passwords-import-file-picker-title",
      passwordsImportFilePickerImportButton:
        "passwords-import-file-picker-import-button",
      passwordsImportFilePickerCsvFilterTitle:
        "passwords-import-file-picker-csv-filter-title",
      passwordsImportFilePickerTsvFilterTitle:
        "passwords-import-file-picker-tsv-filter-title",
      exportPasswordsOSReauthMessage: this.getPlatformFtl(
        "passwords-export-os-auth-dialog-message"
      ),
      passwordsExportFilePickerTitle: "passwords-export-file-picker-title",
      passwordsExportFilePickerDefaultFileName:
        "passwords-export-file-picker-default-filename",
      passwordsExportFilePickerExportButton:
        "passwords-export-file-picker-export-button",
      passwordsExportFilePickerCsvFilterTitle:
        "passwords-export-file-picker-csv-filter-title",
      dismissBreachCommandLabel: "passwords-dismiss-breach-alert-command",
    }).then(strings => {
      const copyCommand = { id: "Copy", label: "command-copy" };
      const editCommand = { id: "Edit", label: "command-edit" };
      const deleteCommand = { id: "Delete", label: "command-delete" };
      const dismissBreachCommand = {
        id: "DismissBreach",
        label: strings.dismissBreachCommandLabel,
      };
      const tooltip = {
        expand: strings.expandSection,
        collapse: strings.collapseSection,
      };
      this.#header = this.createHeaderLine(strings.headerLabel, tooltip);
      this.#header.commands.push(
        { id: "Create", label: "passwords-command-create" },
        {
          id: "ImportFromBrowser",
          label: "passwords-command-import-from-browser",
        },
        { id: "Import", label: "passwords-command-import" },
        { id: "Export", label: "passwords-command-export" },
        { id: "RemoveAll", label: "passwords-command-remove-all" },
        { id: "Settings", label: "passwords-command-settings" },
        { id: "Help", label: "passwords-command-help" },
        { id: "SortByName", label: "passwords-command-sort-name" },
        {
          id: "SortByAlerts",
          label: "passwords-command-sort-alerts",
        }
      );
      this.#header.executeImport = async () =>
        this.#importFromFile(
          strings.passwordsImportFilePickerTitle,
          strings.passwordsImportFilePickerImportButton,
          strings.passwordsImportFilePickerCsvFilterTitle,
          strings.passwordsImportFilePickerTsvFilterTitle
        );

      this.#header.executeImportHelp = () =>
        this.#openLink(IMPORT_FILE_SUPPORT_URL);
      this.#header.executeImportReport = () =>
        this.#openLink(IMPORT_FILE_REPORT_URL);
      this.#header.executeImportFromBrowser = () => this.#importFromBrowser();
      this.#header.executeRemoveAll = () => this.#removeAllPasswords();
      this.#header.executeExport = async () => this.#confirmExportLogins();
      this.#header.executeSettings = () => this.#openLink(PREFERENCES_URL);
      this.#header.executeHelp = () => this.#openLink(SUPPORT_URL);

      this.#exportPasswordsStrings = {
        OSReauthMessage: strings.exportPasswordsOSReauthMessage,
        OSAuthDialogCaption: strings.passwordOSAuthDialogCaption,
        ExportFilePickerTitle: strings.passwordsExportFilePickerTitle,
        FilePickerExportButton: strings.passwordsExportFilePickerExportButton,
        FilePickerDefaultFileName:
          strings.passwordsExportFilePickerDefaultFileName.concat(".csv"),
        FilePickerCsvFilterTitle:
          strings.passwordsExportFilePickerCsvFilterTitle,
      };

      this.#header.executeSortByName = () => {
        if (this.#sortId !== "name") {
          this.#sortId = "name";
          this.#reloadDataSource();
        }
      };

      this.#header.executeSortByAlerts = async () => {
        if (this.#sortId !== "alerts") {
          this.#sortId = "alerts";
          this.#reloadDataSource();
        }
      };

      const openOriginInNewTab = origin => this.#openLink(origin);

      this.#originPrototype = this.prototypeDataLine({
        field: { value: "origin" },
        label: { value: strings.originLabel },
        start: { value: true },
        value: {
          get() {
            return this.record.displayOrigin;
          },
        },
        valueIcon: {
          get() {
            return `page-icon:${this.record.origin}`;
          },
        },
        href: {
          get() {
            return this.record.origin;
          },
        },
        commands: {
          value: [
            { id: "Open", label: "command-open" },
            copyCommand,
            editCommand,
            deleteCommand,
            dismissBreachCommand,
          ],
        },
        executeDismissBreach: {
          value() {
            lazy.LoginBreaches.recordBreachAlertDismissal(this.record.guid);
            delete this.breached;
            this.refreshOnScreen();
          },
        },
        executeCopy: {
          value() {
            this.copyToClipboard(this.record.origin);
          },
        },
        executeDelete: {
          value() {
            this.setLayout({ id: "remove-login" });
          },
        },
        executeOpenLink: {
          value() {
            openOriginInNewTab(this.record.origin);
          },
        },
      });
      this.#usernamePrototype = this.prototypeDataLine({
        field: { value: "username" },
        label: { value: strings.usernameLabel },
        value: {
          get() {
            return this.editingValue ?? this.record.username;
          },
        },
        commands: { value: [copyCommand, editCommand, "-", deleteCommand] },
        executeEdit: {
          value() {
            this.editingValue = this.record.username ?? "";
            this.refreshOnScreen();
          },
        },
        executeSave: {
          value(value) {
            try {
              const modifiedLogin = this.record.clone();
              modifiedLogin.username = value;
              Services.logins.modifyLogin(this.record, modifiedLogin);
            } catch (error) {
              //todo
              console.error("failed to modify login", error);
            }
            this.executeCancel();
          },
        },
      });
      this.#passwordPrototype = this.prototypeDataLine({
        field: { value: "password" },
        label: { value: strings.passwordLabel },
        concealed: { value: true, writable: true },
        end: { value: true },
        value: {
          get() {
            return (
              this.editingValue ??
              (this.concealed ? "••••••••" : this.record.password)
            );
          },
        },
        commands: {
          value: [
            { ...copyCommand, verify: true },
            {
              id: "Reveal",
              label: "command-reveal",
              verify: true,
            },
            { id: "Conceal", label: "command-conceal" },
            editCommand,
            deleteCommand,
          ],
        },
        executeReveal: {
          value() {
            this.concealed = false;
            this.refreshOnScreen();
          },
        },
        executeConceal: {
          value() {
            this.concealed = true;
            this.refreshOnScreen();
          },
        },
        executeCopy: {
          value() {
            this.copyToClipboard(this.record.password);
          },
        },
        executeEdit: {
          value() {
            this.editingValue = this.record.password ?? "";
            this.refreshOnScreen();
          },
        },
        executeSave: {
          value(value) {
            if (!value) {
              return;
            }

            try {
              const modifiedLogin = this.record.clone();
              modifiedLogin.password = value;
              Services.logins.modifyLogin(this.record, modifiedLogin);
            } catch (error) {
              //todo
              console.error("failed to modify login", error);
            }
            this.executeCancel();
          },
        },
      });

      // Sort by origin, then by username, then by GUID
      this.#sortId = "name";
      Services.obs.addObserver(this, "passwordmgr-storage-changed");
      Services.prefs.addObserver("signon.rememberSignons", this);
      Services.prefs.addObserver(
        "signon.management.page.breach-alerts.enabled",
        this
      );
      Services.prefs.addObserver(
        "signon.management.page.vulnerable-passwords.enabled",
        this
      );
      this.#reloadDataSource();
    });
  }

  async #importFromFile(title, buttonLabel, csvTitle, tsvTitle) {
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    const browsingContext = BrowserWindowTracker.getTopWindow().browsingContext;
    let { result, path } = await this.openFilePickerDialog(
      title,
      buttonLabel,
      [
        {
          title: csvTitle,
          extensionPattern: "*.csv",
        },
        {
          title: tsvTitle,
          extensionPattern: "*.tsv",
        },
      ],
      browsingContext
    );

    if (result != Ci.nsIFilePicker.returnCancel) {
      try {
        const summary = await LoginCSVImport.importFromCSV(path);
        const counts = { added: 0, modified: 0 };

        for (const item of summary) {
          if (item.result in counts) {
            counts[item.result] += 1;
          }
        }
        this.setNotification({
          id: "import-success",
          l10nArgs: counts,
          commands: {
            onLinkClick: "ImportReport",
          },
        });
      } catch (e) {
        this.setNotification({
          id: "import-error",
          commands: {
            onLinkClick: "ImportHelp",
            onRetry: "Import",
          },
        });
      }
    }
  }

  async openFilePickerDialog(
    title,
    okButtonLabel,
    appendFilters,
    browsingContext
  ) {
    return new Promise(resolve => {
      let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      fp.init(browsingContext, title, Ci.nsIFilePicker.modeOpen);
      for (const appendFilter of appendFilters) {
        fp.appendFilter(appendFilter.title, appendFilter.extensionPattern);
      }
      fp.appendFilters(Ci.nsIFilePicker.filterAll);
      fp.okButtonLabel = okButtonLabel;
      fp.open(async result => {
        resolve({ result, path: fp.file ? fp.file.path : "" });
      });
    });
  }

  #importFromBrowser() {
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    const browser = BrowserWindowTracker.getTopWindow().gBrowser;
    try {
      lazy.MigrationUtils.showMigrationWizard(browser.ownerGlobal, {
        entrypoint: lazy.MigrationUtils.MIGRATION_ENTRYPOINTS.PASSWORDS,
      });
    } catch (ex) {
      console.error(ex);
    }
  }

  #removeAllPasswords() {
    Services.logins.removeAllLogins();
    this.cancelDialog();
  }

  confirmRemoveLogin([record]) {
    Services.logins.removeLogin(record);
    this.cancelDialog();
  }

  confirmRetryImport() {
    this.#header.executeImport();
    this.cancelDialog();
  }

  async #confirmExportLogins() {
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    const browsingContext = BrowserWindowTracker.getTopWindow().browsingContext;

    const isOSAuthEnabled = LoginHelper.getOSAuthEnabled(
      LoginHelper.OS_AUTH_FOR_PASSWORDS_PREF
    );

    let { isAuthorized, telemetryEvent } = await LoginHelper.requestReauth(
      browsingContext,
      isOSAuthEnabled,
      null, // Prompt regardless of a recent prompt
      this.#exportPasswordsStrings.OSReauthMessage,
      this.#exportPasswordsStrings.OSAuthDialogCaption
    );

    let { name, extra = {}, value = null } = telemetryEvent;
    if (value) {
      extra.value = value;
    }
    Glean.pwmgr[name].record(extra);

    if (!isAuthorized) {
      this.cancelDialog();
      return;
    }
    this.exportFilePickerDialog(browsingContext);
    this.cancelDialog();
  }

  exportFilePickerDialog(browsingContext) {
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    function fpCallback(aResult) {
      if (aResult != Ci.nsIFilePicker.returnCancel) {
        LoginExport.exportAsCSV(fp.file.path);
        Glean.pwmgr.mgmtMenuItemUsedExportComplete.record();
      }
    }
    fp.init(
      browsingContext,
      this.#exportPasswordsStrings.ExportFilePickerTitle,
      Ci.nsIFilePicker.modeSave
    );
    fp.appendFilter(
      this.#exportPasswordsStrings.FilePickerCsvFilterTitle,
      "*.csv"
    );
    fp.appendFilters(Ci.nsIFilePicker.filterAll);
    fp.defaultString = this.#exportPasswordsStrings.FilePickerDefaultFileName;
    fp.defaultExtension = "csv";
    fp.okButtonLabel = this.#exportPasswordsStrings.FilePickerExportButton;
    fp.open(fpCallback);
  }

  #openLink(url) {
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    const browser = BrowserWindowTracker.getTopWindow().gBrowser;
    browser.ownerGlobal.switchToTabHavingURI(url, true, {
      ignoreFragment: "whenComparingAndReplace",
    });
  }

  /**
   * Enumerate all the lines provided by this data source.
   *
   * @param {string} searchText used to filter data
   */
  *enumerateLines(searchText) {
    if (this.#enabled === undefined) {
      // Async Fluent API makes it possible to have data source waiting
      // for the localized strings, which can be detected by undefined in #enabled.
      return;
    }

    yield this.#header;
    if (this.#header.collapsed || !this.#enabled) {
      return;
    }

    const stats = { count: 0, total: 0 };
    searchText = searchText.toUpperCase();
    yield* this.enumerateLinesForMatchingRecords(
      searchText,
      stats,
      login =>
        login.displayOrigin.toUpperCase().includes(searchText) ||
        login.username.toUpperCase().includes(searchText) ||
        login.password.toUpperCase().includes(searchText)
    );

    this.#header.value.statsTotal = stats.total;
  }

  /**
   * Sync lines array with the actual data source.
   * This function reads all logins from the storage, adds or updates lines and
   * removes lines for the removed logins.
   */
  async #reloadDataSource() {
    this.doneReloadDataSource = false;
    this.#enabled = Services.prefs.getBoolPref("signon.rememberSignons");
    if (!this.#enabled) {
      this.#reloadEmptyDataSource();
      this.doneReloadDataSource = true;
      return;
    }

    const logins = await LoginHelper.getAllUserFacingLogins();
    this.beforeReloadingDataSource();

    const breachesMap = lazy.BREACH_ALERTS_ENABLED
      ? await lazy.LoginBreaches.getPotentialBreachesByLoginGUID(logins)
      : new Map();

    const breachedOrVulnerableLogins = logins.filter(
      login =>
        breachesMap.has(login.guid) ||
        lazy.LoginBreaches.isVulnerablePassword(login)
    );

    const filteredLogins =
      this.#sortId === "alerts" ? breachedOrVulnerableLogins : logins;

    filteredLogins.forEach(login => {
      // Similar domains will be grouped together
      // www. will have least effect on the sorting
      const parts = login.displayOrigin.split(".");

      // Exclude TLD domain
      //todo support eTLD and use public suffix here https://publicsuffix.org
      if (parts.length > 1) {
        parts.length -= 1;
      }
      const isLoginBreached = breachesMap.has(login.guid);
      const isLoginVulnerable = lazy.LoginBreaches.isVulnerablePassword(login);

      let alertValue;
      if (isLoginBreached) {
        alertValue = ALERT_VALUES.breached;
      } else if (isLoginVulnerable) {
        alertValue = ALERT_VALUES.vulnerable;
      } else {
        alertValue = ALERT_VALUES.none;
      }

      const domain = parts.reverse().join(".");
      const lineId =
        this.#sortId === "alerts"
          ? `${alertValue}:${domain}:${login.username}:${login.guid}`
          : `${domain}:${login.username}:${login.guid}`;

      let originLine = this.addOrUpdateLine(
        login,
        lineId + "0",
        this.#originPrototype
      );
      this.addOrUpdateLine(login, lineId + "1", this.#usernamePrototype);
      let passwordLine = this.addOrUpdateLine(
        login,
        lineId + "2",
        this.#passwordPrototype
      );

      originLine.breached = isLoginBreached;
      passwordLine.vulnerable = isLoginVulnerable;
    });

    this.#header.value.total = logins.length;
    this.#header.value.alerts = breachedOrVulnerableLogins.length;
    this.afterReloadingDataSource();
    this.doneReloadDataSource = true;
  }

  #reloadEmptyDataSource() {
    this.lines.length = 0;
    //todo: user can enable passwords by activating Passwords header line
    this.#header.value.total = 0;
    this.#header.value.alerts = 0;
    this.refreshAllLinesOnScreen();
  }

  observe(_subj, topic, message) {
    if (
      topic == "passwordmgr-storage-changed" ||
      message == "signon.rememberSignons" ||
      message == "signon.management.page.breach-alerts.enabled" ||
      message == "signon.management.page.vulnerable-passwords.enabled"
    ) {
      this.#reloadDataSource();
    }
  }
}
