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
  _showCallback: undefined,

  async start(aCallback) {
    const enabled = Services.prefs.getBoolPref(
      "browser.crashReports.crashPull"
    );
    if (!enabled) {
      return;
    }

    this._showCallback = aCallback;

    await this.collection();
  },

  /**
   * Setup RemoteSettings listeners
   */
  async collection() {
    try {
      let client = lazy.RemoteSettings(REMOTE_SETTINGS_CRASH_COLLECTION);

      let data = await client.get();
      client.on("sync", async ({ data: { created } }) => {
        await this.checkInterestingCrashesAndMaybeNotify(created);
      });

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
