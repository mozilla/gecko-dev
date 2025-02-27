/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CrashSubmit: "resource://gre/modules/CrashSubmit.sys.mjs",
  CrashServiceUtils: "resource://gre/modules/CrashService.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  RemoteSettingsClient:
    "resource://services-settings/RemoteSettingsClient.sys.mjs",
});

const REMOTE_SETTINGS_CRASH_COLLECTION = "crash-reports-ondemand";

// Remote Settings collections might want a different limit
const PENDING_REMOTE_CRASH_REPORT_DAYS = 90;

export var RemoteSettingsCrashPull = {
  _initialized: false,
  _client: undefined,
  _showCallback: undefined,
  _remoteSettingsCallback: undefined,

  async start(aCallback) {
    if (this._initialized) {
      return;
    }

    const enabled = Services.prefs.getBoolPref(
      "browser.crashReports.crashPull"
    );
    if (!enabled) {
      return;
    }

    this._showCallback = aCallback;

    await this.collection();

    this._initialized = true;
  },

  stop() {
    if (!this._initialized) {
      return;
    }

    this._showCallback = undefined;
    if (this._remoteSettingsCallback) {
      this._client.off("sync", this._remoteSettingsCallback);
      this._remoteSettingsCallback = undefined;
    }
    this._initialized = false;
  },

  async remoteSettingsCallback({ data: { created } }) {
    await this.checkInterestingCrashesAndMaybeNotify(created);
  },

  /**
   * Setup RemoteSettings listeners
   */
  async collection() {
    // Bind once so we can on("sync") then off("sync") with the same callback
    this._remoteSettingsCallback = this.remoteSettingsCallback.bind(this);

    try {
      this._client = lazy.RemoteSettings(REMOTE_SETTINGS_CRASH_COLLECTION);
      this._client.on("sync", this._remoteSettingsCallback);

      let data = await this._client.get();
      await this.checkInterestingCrashesAndMaybeNotify(data);
    } catch (ex) {
      if (!(ex instanceof lazy.RemoteSettingsClient.UnknownCollectionError)) {
        throw ex;
      }
    }
  },

  async getPendingSha256(dateLimit) {
    // XXX: this will not report .ignore file; are we OK?
    let pendingIDs = await lazy.CrashSubmit.pendingIDs(dateLimit);
    if (pendingIDs.length === 0) {
      return [];
    }

    const uAppDataPath = Services.dirsvc.get("UAppData", Ci.nsIFile).path;
    const pendingDir = PathUtils.join(uAppDataPath, "Crash Reports", "pending");

    let pendingSHA256 = await Promise.all(
      pendingIDs
        .map(id => {
          return { id, path: PathUtils.join(pendingDir, `${id}.dmp`) };
        })
        .filter(async e => {
          return await IOUtils.exists(e.path);
        })
        .map(async e => {
          return {
            id: e.id,
            sha256: await lazy.CrashServiceUtils.computeMinidumpHash(e.path),
          };
        })
    );

    return pendingSHA256;
  },

  async checkForInterestingUnsubmittedCrash(records) {
    let dateLimit = new Date();
    dateLimit.setDate(dateLimit.getDate() - PENDING_REMOTE_CRASH_REPORT_DAYS);

    const recordHashes = new Set(records.flatMap(entry => entry.hashes));
    let pendingSHA256 = await this.getPendingSha256(dateLimit);
    let matches = pendingSHA256
      .filter(pendingCrash => recordHashes.has(pendingCrash.sha256))
      .map(pendingCrash => pendingCrash.id);

    return matches;
  },

  async checkInterestingCrashesAndMaybeNotify(records) {
    const neverShowAgain = Services.prefs.getBoolPref(
      "browser.crashReports.requestedNeverShowAgain"
    );
    if (neverShowAgain) {
      return;
    }

    let matches = await this.checkForInterestingUnsubmittedCrash(records);
    if (this._showCallback && matches.length) {
      await this._showCallback(matches, true);
    }
  },
};
