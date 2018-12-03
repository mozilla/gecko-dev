/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global __URI__ */

"use strict";

var EXPORTED_SYMBOLS = [
  "RemoteSettings",
  "jexlFilterFunc",
  "remoteSettingsBroadcastHandler",
];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "UptakeTelemetry",
                               "resource://services-common/uptake-telemetry.js");
ChromeUtils.defineModuleGetter(this, "pushBroadcastService",
                               "resource://gre/modules/PushBroadcastService.jsm");
ChromeUtils.defineModuleGetter(this, "RemoteSettingsClient",
                               "resource://services-settings/RemoteSettingsClient.jsm");
ChromeUtils.defineModuleGetter(this, "Utils",
                               "resource://services-settings/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "FilterExpressions",
                               "resource://gre/modules/components-utils/FilterExpressions.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

const PREF_SETTINGS_DEFAULT_BUCKET     = "services.settings.default_bucket";
const PREF_SETTINGS_BRANCH             = "services.settings.";
const PREF_SETTINGS_SERVER             = "server";
const PREF_SETTINGS_DEFAULT_SIGNER     = "default_signer";
const PREF_SETTINGS_SERVER_BACKOFF     = "server.backoff";
const PREF_SETTINGS_CHANGES_PATH       = "changes.path";
const PREF_SETTINGS_LAST_UPDATE        = "last_update_seconds";
const PREF_SETTINGS_LAST_ETAG          = "last_etag";
const PREF_SETTINGS_CLOCK_SKEW_SECONDS = "clock_skew_seconds";
const PREF_SETTINGS_LOAD_DUMP          = "load_dump";


// Telemetry update source identifier.
const TELEMETRY_HISTOGRAM_KEY = "settings-changes-monitoring";
// Push broadcast id.
const BROADCAST_ID = "remote-settings/monitor_changes";

XPCOMUtils.defineLazyGetter(this, "gPrefs", () => {
  return Services.prefs.getBranch(PREF_SETTINGS_BRANCH);
});
XPCOMUtils.defineLazyPreferenceGetter(this, "gServerURL", PREF_SETTINGS_BRANCH + PREF_SETTINGS_SERVER);
XPCOMUtils.defineLazyPreferenceGetter(this, "gChangesPath", PREF_SETTINGS_BRANCH + PREF_SETTINGS_CHANGES_PATH);

/**
 * Default entry filtering function, in charge of excluding remote settings entries
 * where the JEXL expression evaluates into a falsy value.
 * @param {Object}            entry       The Remote Settings entry to be excluded or kept.
 * @param {ClientEnvironment} environment Information about version, language, platform etc.
 * @returns {?Object} the entry or null if excluded.
 */
async function jexlFilterFunc(entry, environment) {
  const { filter_expression } = entry;
  if (!filter_expression) {
    return entry;
  }
  let result;
  try {
    const context = {
      environment,
    };
    result = await FilterExpressions.eval(filter_expression, context);
  } catch (e) {
    Cu.reportError(e);
  }
  return result ? entry : null;
}


function remoteSettingsFunction() {
  const _clients = new Map();

  // If not explicitly specified, use the default signer.
  const defaultSigner = gPrefs.getCharPref(PREF_SETTINGS_DEFAULT_SIGNER);
  const defaultOptions = {
    bucketNamePref: PREF_SETTINGS_DEFAULT_BUCKET,
    signerName: defaultSigner,
    filterFunc: jexlFilterFunc,
  };

  /**
   * RemoteSettings constructor.
   *
   * @param {String} collectionName The remote settings identifier
   * @param {Object} options Advanced options
   * @returns {RemoteSettingsClient} An instance of a Remote Settings client.
   */
  const remoteSettings = function(collectionName, options) {
    // Get or instantiate a remote settings client.
    if (!_clients.has(collectionName)) {
      // Register a new client!
      const c = new RemoteSettingsClient(collectionName, { ...defaultOptions, ...options });
      // Store instance for later call.
      _clients.set(collectionName, c);
      // Invalidate the polling status, since we want the new collection to
      // be taken into account.
      gPrefs.clearUserPref(PREF_SETTINGS_LAST_ETAG);
    }
    return _clients.get(collectionName);
  };

  Object.defineProperty(remoteSettings, "pollingEndpoint", {
    get() {
      return gServerURL + gChangesPath;
    },
  });

  /**
   * Internal helper to retrieve existing instances of clients or new instances
   * with default options if possible, or `null` if bucket/collection are unknown.
   */
  async function _client(bucketName, collectionName) {
    // Check if a client was registered for this bucket/collection. Potentially
    // with some specific options like signer, filter function etc.
    const client = _clients.get(collectionName);
    if (client && client.bucketName == bucketName) {
      return client;
    }
    // There was no client registered for this collection, but it's the main bucket,
    // therefore we can instantiate a client with the default options.
    // So if we have a local database or if we ship a JSON dump, then it means that
    // this client is known but it was not registered yet (eg. calling module not "imported" yet).
    if (bucketName == Services.prefs.getCharPref(PREF_SETTINGS_DEFAULT_BUCKET)) {
      const c = new RemoteSettingsClient(collectionName, defaultOptions);
      const [dbExists, localDump] = await Promise.all([
        Utils.hasLocalData(c),
        Utils.hasLocalDump(bucketName, collectionName),
      ]);
      if (dbExists || localDump) {
        return c;
      }
    }
    // Else, we cannot return a client insttance because we are not able to synchronize data in specific buckets.
    // Mainly because we cannot guess which `signerName` has to be used for example.
    // And we don't want to synchronize data for collections in the main bucket that are
    // completely unknown (ie. no database and no JSON dump).
    return null;
  }

  /**
   * Main polling method, called by the ping mechanism.
   *
   * @param {Object} options
.  * @param {Object} options.expectedTimestamp (optional) The expected timestamp to be received — used by servers for cache busting.
   * @returns {Promise} or throws error if something goes wrong.
   */
  remoteSettings.pollChanges = async ({ expectedTimestamp } = {}) => {
    // Check if the server backoff time is elapsed.
    if (gPrefs.prefHasUserValue(PREF_SETTINGS_SERVER_BACKOFF)) {
      const backoffReleaseTime = gPrefs.getCharPref(PREF_SETTINGS_SERVER_BACKOFF);
      const remainingMilliseconds = parseInt(backoffReleaseTime, 10) - Date.now();
      if (remainingMilliseconds > 0) {
        // Backoff time has not elapsed yet.
        UptakeTelemetry.report(TELEMETRY_HISTOGRAM_KEY,
                               UptakeTelemetry.STATUS.BACKOFF);
        throw new Error(`Server is asking clients to back off; retry in ${Math.ceil(remainingMilliseconds / 1000)}s.`);
      } else {
        gPrefs.clearUserPref(PREF_SETTINGS_SERVER_BACKOFF);
      }
    }

    const lastEtag = gPrefs.getCharPref(PREF_SETTINGS_LAST_ETAG, "");

    let pollResult;
    try {
      pollResult = await Utils.fetchLatestChanges(remoteSettings.pollingEndpoint, { expectedTimestamp, lastEtag });
    } catch (e) {
      // Report polling error to Uptake Telemetry.
      let report;
      if (/Server/.test(e.message)) {
        report = UptakeTelemetry.STATUS.SERVER_ERROR;
      } else if (/NetworkError/.test(e.message)) {
        report = UptakeTelemetry.STATUS.NETWORK_ERROR;
      } else {
        report = UptakeTelemetry.STATUS.UNKNOWN_ERROR;
      }
      UptakeTelemetry.report(TELEMETRY_HISTOGRAM_KEY, report);
      // No need to go further.
      throw new Error(`Polling for changes failed: ${e.message}.`);
    }

    const {serverTimeMillis, changes, currentEtag, backoffSeconds} = pollResult;

    // Report polling success to Uptake Telemetry.
    const report = changes.length == 0 ? UptakeTelemetry.STATUS.UP_TO_DATE
                                       : UptakeTelemetry.STATUS.SUCCESS;
    UptakeTelemetry.report(TELEMETRY_HISTOGRAM_KEY, report);

    // Check if the server asked the clients to back off (for next poll).
    if (backoffSeconds) {
      const backoffReleaseTime = Date.now() + backoffSeconds * 1000;
      gPrefs.setCharPref(PREF_SETTINGS_SERVER_BACKOFF, backoffReleaseTime);
    }

    // Record new update time and the difference between local and server time.
    // Negative clockDifference means local time is behind server time
    // by the absolute of that value in seconds (positive means it's ahead)
    const clockDifference = Math.floor((Date.now() - serverTimeMillis) / 1000);
    gPrefs.setIntPref(PREF_SETTINGS_CLOCK_SKEW_SECONDS, clockDifference);
    const checkedServerTimeInSeconds = Math.round(serverTimeMillis / 1000);
    gPrefs.setIntPref(PREF_SETTINGS_LAST_UPDATE, checkedServerTimeInSeconds);


    const loadDump = gPrefs.getBoolPref(PREF_SETTINGS_LOAD_DUMP, true);

    // Iterate through the collections version info and initiate a synchronization
    // on the related remote settings client.
    let firstError;
    for (const change of changes) {
      const { bucket, collection, last_modified } = change;

      const client = await _client(bucket, collection);
      if (!client) {
        continue;
      }
      // Start synchronization! It will be a no-op if the specified `lastModified` equals
      // the one in the local database.
      try {
        await client.maybeSync(last_modified, { loadDump });
        // Save last time this client was successfully synced.
        Services.prefs.setIntPref(client.lastCheckTimePref, checkedServerTimeInSeconds);
      } catch (e) {
        if (!firstError) {
          firstError = e;
          firstError.details = change;
        }
      }
    }
    if (firstError) {
      // cause the promise to reject by throwing the first observed error
      throw firstError;
    }

    // Save current Etag for next poll.
    if (currentEtag) {
      gPrefs.setCharPref(PREF_SETTINGS_LAST_ETAG, currentEtag);
    }

    Services.obs.notifyObservers(null, "remote-settings-changes-polled");
  };

  /**
   * Returns an object with polling status information and the list of
   * known remote settings collections.
   */
  remoteSettings.inspect = async () => {
    const { changes, currentEtag: serverTimestamp } = await Utils.fetchLatestChanges(remoteSettings.pollingEndpoint);

    const collections = await Promise.all(changes.map(async (change) => {
      const { bucket, collection, last_modified: serverTimestamp } = change;
      const client = await _client(bucket, collection);
      if (!client) {
        return null;
      }
      const kintoCol = await client.openCollection();
      const localTimestamp = await kintoCol.db.getLastModified();
      const lastCheck = Services.prefs.getIntPref(client.lastCheckTimePref, 0);
      return {
        bucket,
        collection,
        localTimestamp,
        serverTimestamp,
        lastCheck,
        signerName: client.signerName,
      };
    }));

    return {
      serverURL: gServerURL,
      serverTimestamp,
      localTimestamp: gPrefs.getCharPref(PREF_SETTINGS_LAST_ETAG, null),
      lastCheck: gPrefs.getIntPref(PREF_SETTINGS_LAST_UPDATE, 0),
      mainBucket: Services.prefs.getCharPref(PREF_SETTINGS_DEFAULT_BUCKET),
      defaultSigner,
      collections: collections.filter(c => !!c),
    };
  };

  /**
   * Startup function called from nsBrowserGlue.
   */
  remoteSettings.init = () => {
    // Hook the Push broadcast and RemoteSettings polling.
    // When we start on a new profile there will be no ETag stored.
    // Use an arbitrary ETag that is guaranteed not to occur.
    // This will trigger a broadcast message but that's fine because we
    // will check the changes on each collection and retrieve only the
    // changes (e.g. nothing if we have a dump with the same data).
    const currentVersion = gPrefs.getStringPref(PREF_SETTINGS_LAST_ETAG, "\"0\"");
    const moduleInfo = {
      moduleURI: __URI__,
      symbolName: "remoteSettingsBroadcastHandler",
    };
    pushBroadcastService.addListener(BROADCAST_ID, currentVersion, moduleInfo);
  };

  return remoteSettings;
}

var RemoteSettings = remoteSettingsFunction();

var remoteSettingsBroadcastHandler = {
  async receivedBroadcastMessage(data, broadcastID) {
    return RemoteSettings.pollChanges({ expectedTimestamp: data });
  },
};
