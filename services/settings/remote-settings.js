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
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm", {});
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch", "indexedDB"]);

ChromeUtils.defineModuleGetter(this, "Kinto",
                               "resource://services-common/kinto-offline-client.js");
ChromeUtils.defineModuleGetter(this, "KintoHttpClient",
                               "resource://services-common/kinto-http-client.js");
ChromeUtils.defineModuleGetter(this, "CanonicalJSON",
                               "resource://gre/modules/CanonicalJSON.jsm");
ChromeUtils.defineModuleGetter(this, "UptakeTelemetry",
                               "resource://services-common/uptake-telemetry.js");
ChromeUtils.defineModuleGetter(this, "ClientEnvironmentBase",
                               "resource://gre/modules/components-utils/ClientEnvironment.jsm");
ChromeUtils.defineModuleGetter(this, "FilterExpressions",
                               "resource://gre/modules/components-utils/FilterExpressions.jsm");
ChromeUtils.defineModuleGetter(this, "pushBroadcastService",
                               "resource://gre/modules/PushBroadcastService.jsm");

const PREF_SETTINGS_DEFAULT_BUCKET     = "services.settings.default_bucket";
const PREF_SETTINGS_BRANCH             = "services.settings.";
const PREF_SETTINGS_SERVER             = "server";
const PREF_SETTINGS_DEFAULT_SIGNER     = "default_signer";
const PREF_SETTINGS_VERIFY_SIGNATURE   = "verify_signature";
const PREF_SETTINGS_SERVER_BACKOFF     = "server.backoff";
const PREF_SETTINGS_CHANGES_PATH       = "changes.path";
const PREF_SETTINGS_LAST_UPDATE        = "last_update_seconds";
const PREF_SETTINGS_LAST_ETAG          = "last_etag";
const PREF_SETTINGS_CLOCK_SKEW_SECONDS = "clock_skew_seconds";
const PREF_SETTINGS_LOAD_DUMP          = "load_dump";

// Telemetry update source identifier.
const TELEMETRY_HISTOGRAM_KEY = "settings-changes-monitoring";

const INVALID_SIGNATURE = "Invalid content/signature";
const MISSING_SIGNATURE = "Missing signature";

XPCOMUtils.defineLazyGetter(this, "gPrefs", () => {
  return Services.prefs.getBranch(PREF_SETTINGS_BRANCH);
});
XPCOMUtils.defineLazyPreferenceGetter(this, "gVerifySignature", PREF_SETTINGS_BRANCH + PREF_SETTINGS_VERIFY_SIGNATURE, true);
XPCOMUtils.defineLazyPreferenceGetter(this, "gServerURL", PREF_SETTINGS_BRANCH + PREF_SETTINGS_SERVER);
XPCOMUtils.defineLazyPreferenceGetter(this, "gChangesPath", PREF_SETTINGS_BRANCH + PREF_SETTINGS_CHANGES_PATH);

/**
 * cacheProxy returns an object Proxy that will memoize properties of the target.
 */
function cacheProxy(target) {
  const cache = new Map();
  return new Proxy(target, {
    get(target, prop, receiver) {
      if (!cache.has(prop)) {
        cache.set(prop, target[prop]);
      }
      return cache.get(prop);
    },
  });
}

class ClientEnvironment extends ClientEnvironmentBase {
  static get appID() {
    // eg. Firefox is "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}".
    Services.appinfo.QueryInterface(Ci.nsIXULAppInfo);
    return Services.appinfo.ID;
  }

  static get toolkitVersion() {
    Services.appinfo.QueryInterface(Ci.nsIPlatformInfo);
    return Services.appinfo.platformVersion;
  }
}

/**
 * Default entry filtering function, in charge of excluding remote settings entries
 * where the JEXL expression evaluates into a falsy value.
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


function mergeChanges(collection, localRecords, changes) {
  const records = {};
  // Local records by id.
  localRecords.forEach((record) => records[record.id] = collection.cleanLocalFields(record));
  // All existing records are replaced by the version from the server.
  changes.forEach((record) => records[record.id] = record);

  return Object.values(records)
    // Filter out deleted records.
    .filter((record) => !record.deleted)
    // Sort list by record id.
    .sort((a, b) => {
      if (a.id < b.id) {
        return -1;
      }
      return a.id > b.id ? 1 : 0;
    });
}


async function fetchCollectionMetadata(remote, collection, expectedTimestamp) {
  const client = new KintoHttpClient(remote);
  //
  // XXX: https://github.com/Kinto/kinto-http.js/issues/307
  //
  const { signature } = await client.bucket(collection.bucket)
                                    .collection(collection.name)
                                    .getData({ query: { _expected: expectedTimestamp }});
  return signature;
}

async function fetchRemoteCollection(collection, expectedTimestamp) {
  const client = new KintoHttpClient(gServerURL);
  return client.bucket(collection.bucket)
           .collection(collection.name)
           .listRecords({ sort: "id", filters: { _expected: expectedTimestamp } });
}

/**
 * Fetch the list of remote collections and their timestamp.
 * @param {String} url               The poll URL (eg. `http://${server}{pollingEndpoint}`)
 * @param {String} lastEtag          (optional) The Etag of the latest poll to be matched
 *                                    by the server (eg. `"123456789"`).
 * @param {int}    expectedTimestamp The timestamp that the server is supposed to return.
 *                                   We obtained it from the Megaphone notification payload,
 *                                   and we use it only for cache busting (Bug 1497159).
 */
async function fetchLatestChanges(url, lastEtag, expectedTimestamp) {
  //
  // Fetch the list of changes objects from the server that looks like:
  // {"data":[
  //   {
  //     "host":"kinto-ota.dev.mozaws.net",
  //     "last_modified":1450717104423,
  //     "bucket":"blocklists",
  //     "collection":"certificates"
  //    }]}

  // Use ETag to obtain a `304 Not modified` when no change occurred,
  // and `?_since` parameter to only keep entries that weren't processed yet.
  const headers = {};
  const params = {};
  if (lastEtag) {
    headers["If-None-Match"] = lastEtag;
    params._since = lastEtag;
  }
  if (expectedTimestamp) {
    params._expected = expectedTimestamp;
  }
  if (params) {
    url += "?" + Object.entries(params).map(([k, v]) => `${k}=${encodeURIComponent(v)}`).join("&");
  }
  const response = await fetch(url, {headers});

  let changes = [];
  // If no changes since last time, go on with empty list of changes.
  if (response.status != 304) {
    let payload;
    try {
      payload = await response.json();
    } catch (e) {
      payload = e.message;
    }

    if (!payload.hasOwnProperty("data")) {
      // If the server is failing, the JSON response might not contain the
      // expected data. For example, real server errors (Bug 1259145)
      // or dummy local server for tests (Bug 1481348)
      const is404FromCustomServer = response.status == 404 && gPrefs.prefHasUserValue(PREF_SETTINGS_SERVER);
      if (!is404FromCustomServer) {
        throw new Error(`Server error ${response.status} ${response.statusText}: ${JSON.stringify(payload)}`);
      }
    } else {
      changes = payload.data;
    }
  }
  // The server should always return ETag. But we've had situations where the CDN
  // was interfering.
  const currentEtag = response.headers.has("ETag") ? response.headers.get("ETag") : undefined;
  let serverTimeMillis = Date.parse(response.headers.get("Date"));
  // Since the response is served via a CDN, the Date header value could have been cached.
  const ageSeconds = response.headers.has("Age") ? parseInt(response.headers.get("Age"), 10) : 0;
  serverTimeMillis += ageSeconds * 1000;

  // Check if the server asked the clients to back off.
  let backoffSeconds;
  if (response.headers.has("Backoff")) {
    const value = parseInt(response.headers.get("Backoff"), 10);
    if (!isNaN(value)) {
      backoffSeconds = value;
    }
  }

  return {changes, currentEtag, serverTimeMillis, backoffSeconds};
}

/**
 * Load the the JSON file distributed with the release for this collection.
 * @param {String}  bucket
 * @param {String}  collection
 * @param {Object}  options
 * @param {boolean} options.ignoreMissing Do not throw an error if the file is missing.
 */
async function loadDumpFile(bucket, collection, { ignoreMissing = true } = {}) {
  const fileURI = `resource://app/defaults/settings/${bucket}/${collection}.json`;
  let response;
  try {
    // Will throw NetworkError is folder/file is missing.
    response = await fetch(fileURI);
    if (!response.ok) {
      throw new Error(`Could not read from '${fileURI}'`);
    }
    // Will throw if JSON is invalid.
    return response.json();
  } catch (e) {
    // A missing file is reported as "NetworError" (see Bug 1493709)
    if (!ignoreMissing || !/NetworkError/.test(e.message)) {
      throw e;
    }
  }
  return { data: [] };
}


class RemoteSettingsClient {

  constructor(collectionName, { bucketNamePref, signerName, filterFunc = jexlFilterFunc, localFields = [], lastCheckTimePref }) {
    this.collectionName = collectionName;
    this.signerName = signerName;
    this.filterFunc = filterFunc;
    this.localFields = localFields;
    this._lastCheckTimePref = lastCheckTimePref;

    // The bucket preference value can be changed (eg. `main` to `main-preview`) in order
    // to preview the changes to be approved in a real client.
    this.bucketNamePref = bucketNamePref;
    XPCOMUtils.defineLazyPreferenceGetter(this, "bucketName", this.bucketNamePref);

    this._listeners = new Map();
    this._listeners.set("sync", []);
  }

  get identifier() {
    return `${this.bucketName}/${this.collectionName}`;
  }

  get lastCheckTimePref() {
    return this._lastCheckTimePref || `services.settings.${this.bucketName}.${this.collectionName}.last_check`;
  }

  /**
   * Event emitter: will execute the registered listeners in the order and
   * sequentially.
   *
   * Note: we don't use `toolkit/modules/EventEmitter` because we want to throw
   * an error when a listener fails to execute.
   *
   * @param {string} event    the event name
   * @param {Object} payload  the event payload to call the listeners with
   */
  async emit(event, payload) {
    const callbacks = this._listeners.get("sync");
    let firstError;
    for (const cb of callbacks) {
      try {
        await cb(payload);
      } catch (e) {
        firstError = e;
      }
    }
    if (firstError) {
      throw firstError;
    }
  }

  on(event, callback) {
    if (!this._listeners.has(event)) {
      throw new Error(`Unknown event type ${event}`);
    }
    this._listeners.get(event).push(callback);
  }

  off(event, callback) {
    if (!this._listeners.has(event)) {
      throw new Error(`Unknown event type ${event}`);
    }
    const callbacks = this._listeners.get(event);
    const i = callbacks.indexOf(callback);
    if (i < 0) {
      throw new Error(`Unknown callback`);
    } else {
      callbacks.splice(i, 1);
    }
  }

  /**
   * Open the underlying Kinto collection, using the appropriate adapter and
   * options.
   */
  async openCollection() {
    if (!this._kinto) {
      this._kinto = new Kinto({
        bucket: this.bucketName,
        adapter: Kinto.adapters.IDB,
        adapterOptions: { dbName: "remote-settings", migrateOldData: false },
      });
    }
    const options = {
      localFields: this.localFields,
      bucket: this.bucketName,
    };
    return this._kinto.collection(this.collectionName, options);
  }

  /**
   * Lists settings.
   *
   * @param  {Object} options         The options object.
   * @param  {Object} options.filters Filter the results (default: `{}`).
   * @param  {Object} options.order   The order to apply   (default: `-last_modified`).
   * @return {Promise}
   */
  async get(options = {}) {
    // In Bug 1451031, we will do some jexl filtering to limit the list items
    // whose target is matched.
    const { filters = {}, order } = options;
    const c = await this.openCollection();

    const timestamp = await c.db.getLastModified();
    // If the local database was never synchronized, then we attempt to load
    // a packaged JSON dump.
    if (timestamp == null) {
      try {
        const { data } = await loadDumpFile(this.bucketName, this.collectionName);
        await c.loadDump(data);
      } catch (e) {
        // Report but return an empty list since there will be no data anyway.
        Cu.reportError(e);
        return [];
      }
    }

    const { data } = await c.list({ filters, order });
    return this._filterEntries(data);
  }

  /**
   * Synchronize from Kinto server, if necessary.
   *
   * @param {int}  expectedTimestamp       the lastModified date (on the server) for
                                      the remote collection.
   * @param {Date}   serverTime       the current date return by the server.
   * @param {Object} options          additional advanced options.
   * @param {bool}   options.loadDump load initial dump from disk on first sync (default: true)
   * @return {Promise}                which rejects on sync or process failure.
   */
  async maybeSync(expectedTimestamp, serverTime, options = { loadDump: true }) {
    const {loadDump} = options;

    let reportStatus = null;
    try {
      const collection = await this.openCollection();
      // Synchronize remote data into a local Sqlite DB.
      let collectionLastModified = await collection.db.getLastModified();

      // If there is no data currently in the collection, attempt to import
      // initial data from the application defaults.
      // This allows to avoid synchronizing the whole collection content on
      // cold start.
      if (!collectionLastModified && loadDump) {
        try {
          const initialData = await loadDumpFile(this.bucketName, this.collectionName);
          await collection.loadDump(initialData.data);
          collectionLastModified = await collection.db.getLastModified();
        } catch (e) {
          // Report but go-on.
          Cu.reportError(e);
        }
      }

      // If the data is up to date, there's no need to sync. We still need
      // to record the fact that a check happened.
      if (expectedTimestamp <= collectionLastModified) {
        this._updateLastCheck(serverTime);
        reportStatus = UptakeTelemetry.STATUS.UP_TO_DATE;
        return;
      }

      // If there is a `signerName` and collection signing is enforced, add a
      // hook for incoming changes that validates the signature.
      if (this.signerName && gVerifySignature) {
        collection.hooks["incoming-changes"] = [(payload, collection) => {
          return this._validateCollectionSignature(payload, collection, { expectedTimestamp });
        }];
      }

      // Fetch changes from server.
      let syncResult;
      try {
        // Server changes have priority during synchronization.
        const strategy = Kinto.syncStrategy.SERVER_WINS;
        //
        // XXX: https://github.com/Kinto/kinto.js/issues/859
        //
        syncResult = await collection.sync({ remote: gServerURL, strategy, expectedTimestamp });
        const { ok } = syncResult;
        if (!ok) {
          // Some synchronization conflicts occured.
          reportStatus = UptakeTelemetry.STATUS.CONFLICT_ERROR;
          throw new Error("Sync failed");
        }
      } catch (e) {
        if (e.message == INVALID_SIGNATURE) {
          // Signature verification failed during synchronzation.
          reportStatus = UptakeTelemetry.STATUS.SIGNATURE_ERROR;
          // if sync fails with a signature error, it's likely that our
          // local data has been modified in some way.
          // We will attempt to fix this by retrieving the whole
          // remote collection.
          const payload = await fetchRemoteCollection(collection, expectedTimestamp);
          try {
            await this._validateCollectionSignature(payload, collection, { expectedTimestamp, ignoreLocal: true });
          } catch (e) {
            reportStatus = UptakeTelemetry.STATUS.SIGNATURE_RETRY_ERROR;
            throw e;
          }

          // The signature is good (we haven't thrown).
          // Now we will Inspect what we had locally.
          const { data: oldData } = await collection.list();

          // We build a sync result as if a diff-based sync was performed.
          syncResult = { created: [], updated: [], deleted: [] };

          // If the remote last_modified is newer than the local last_modified,
          // replace the local data
          const localLastModified = await collection.db.getLastModified();
          if (payload.last_modified >= localLastModified) {
            const { data: newData } = payload;
            await collection.clear();
            await collection.loadDump(newData);

            // Compare local and remote to populate the sync result
            const oldById = new Map(oldData.map(e => [e.id, e]));
            for (const r of newData) {
              const old = oldById.get(r.id);
              if (old) {
                if (old.last_modified != r.last_modified) {
                  syncResult.updated.push({ old, new: r });
                }
                oldById.delete(r.id);
              } else {
                syncResult.created.push(r);
              }
            }
            // Records that remain in our map now are those missing from remote
            syncResult.deleted = Array.from(oldById.values());
          }

        } else {
          // The sync has thrown, it can be related to metadata, network or a general error.
          if (e.message == MISSING_SIGNATURE) {
            // Collection metadata has no signature info, no need to retry.
            reportStatus = UptakeTelemetry.STATUS.SIGNATURE_ERROR;
          } else if (/NetworkError/.test(e.message)) {
            reportStatus = UptakeTelemetry.STATUS.NETWORK_ERROR;
          } else if (/Backoff/.test(e.message)) {
            reportStatus = UptakeTelemetry.STATUS.BACKOFF;
          } else {
            reportStatus = UptakeTelemetry.STATUS.SYNC_ERROR;
          }
          throw e;
        }
      }

      // Handle the obtained records (ie. apply locally through events).
      // Build the event data list. It should be filtered (ie. by application target)
      const { created: allCreated, updated: allUpdated, deleted: allDeleted } = syncResult;
      const [created, deleted, updatedFiltered] = await Promise.all(
          [allCreated, allDeleted, allUpdated.map(e => e.new)].map(this._filterEntries.bind(this))
        );
      // For updates, keep entries whose updated form is matches the target.
      const updatedFilteredIds = new Set(updatedFiltered.map(e => e.id));
      const updated = allUpdated.filter(({ new: { id } }) => updatedFilteredIds.has(id));

      // If every changed entry is filtered, we don't even fire the event.
      if (created.length || updated.length || deleted.length) {
        // Read local collection of records (also filtered).
        const { data: allData } = await collection.list();
        const current = await this._filterEntries(allData);
        const payload = { data: { current, created, updated, deleted } };
        try {
          await this.emit("sync", payload);
        } catch (e) {
          reportStatus = UptakeTelemetry.STATUS.APPLY_ERROR;
          throw e;
        }
      }

      // Track last update.
      this._updateLastCheck(serverTime);

    } catch (e) {
      // No specific error was tracked, mark it as unknown.
      if (reportStatus === null) {
        reportStatus = UptakeTelemetry.STATUS.UNKNOWN_ERROR;
      }
      throw e;
    } finally {
      // No error was reported, this is a success!
      if (reportStatus === null) {
        reportStatus = UptakeTelemetry.STATUS.SUCCESS;
      }
      // Report success/error status to Telemetry.
      UptakeTelemetry.report(this.identifier, reportStatus);
    }
  }

  async _validateCollectionSignature(payload, collection, options = {}) {
    const { expectedTimestamp, ignoreLocal } = options;
    // this is a content-signature field from an autograph response.
    const signaturePayload = await fetchCollectionMetadata(gServerURL, collection, expectedTimestamp);
    if (!signaturePayload) {
      throw new Error(MISSING_SIGNATURE);
    }
    const {x5u, signature} = signaturePayload;
    const certChainResponse = await fetch(x5u);
    const certChain = await certChainResponse.text();

    const verifier = Cc["@mozilla.org/security/contentsignatureverifier;1"]
                       .createInstance(Ci.nsIContentSignatureVerifier);

    let toSerialize;
    if (ignoreLocal) {
      toSerialize = {
        last_modified: `${payload.last_modified}`,
        data: payload.data,
      };
    } else {
      const {data: localRecords} = await collection.list();
      const records = mergeChanges(collection, localRecords, payload.changes);
      toSerialize = {
        last_modified: `${payload.lastModified}`,
        data: records,
      };
    }

    const serialized = CanonicalJSON.stringify(toSerialize);

    if (verifier.verifyContentSignature(serialized, "p384ecdsa=" + signature,
                                        certChain,
                                        this.signerName)) {
      // In case the hash is valid, apply the changes locally.
      return payload;
    }
    throw new Error(INVALID_SIGNATURE);
  }

  /**
   * Save last time server was checked in users prefs.
   *
   * @param {Date} serverTime   the current date return by server.
   */
  _updateLastCheck(serverTime) {
    const checkedServerTimeInSeconds = Math.round(serverTime / 1000);
    Services.prefs.setIntPref(this.lastCheckTimePref, checkedServerTimeInSeconds);
  }

  async _filterEntries(data) {
    // Filter entries for which calls to `this.filterFunc` returns null.
    if (!this.filterFunc) {
      return data;
    }
    const environment = cacheProxy(ClientEnvironment);
    const dataPromises = data.map(e => this.filterFunc(e, environment));
    const results = await Promise.all(dataPromises);
    return results.filter(v => !!v);
  }
}

/**
 * Check if local data exist for the specified client.
 *
 * @param {RemoteSettingsClient} client
 * @return {bool} Whether it exists or not.
 */
async function hasLocalData(client) {
  const kintoCol = await client.openCollection();
  const timestamp = await kintoCol.db.getLastModified();
  return timestamp !== null;
}

/**
 * Check if we ship a JSON dump for the specified bucket and collection.
 *
 * @param {String} bucket
 * @param {String} collection
 * @return {bool} Whether it is present or not.
 */
async function hasLocalDump(bucket, collection) {
  try {
    await loadDumpFile(bucket, collection, {ignoreMissing: false});
    return true;
  } catch (e) {
    return false;
  }
}


function remoteSettingsFunction() {
  const _clients = new Map();

  // If not explicitly specified, use the default signer.
  const defaultSigner = gPrefs.getCharPref(PREF_SETTINGS_DEFAULT_SIGNER);
  const defaultOptions = {
    bucketNamePref: PREF_SETTINGS_DEFAULT_BUCKET,
    signerName: defaultSigner,
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
        hasLocalData(c),
        hasLocalDump(bucketName, collectionName),
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

    let lastEtag;
    if (gPrefs.prefHasUserValue(PREF_SETTINGS_LAST_ETAG)) {
      lastEtag = gPrefs.getCharPref(PREF_SETTINGS_LAST_ETAG);
    }

    let pollResult;
    try {
      pollResult = await fetchLatestChanges(remoteSettings.pollingEndpoint, lastEtag, expectedTimestamp);
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
    gPrefs.setIntPref(PREF_SETTINGS_LAST_UPDATE, serverTimeMillis / 1000);

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
        await client.maybeSync(last_modified, serverTimeMillis, {loadDump});
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
    const { changes, currentEtag: serverTimestamp } = await fetchLatestChanges(remoteSettings.pollingEndpoint);

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
    const broadcastID = "remote-settings/monitor_changes";
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
    pushBroadcastService.addListener(broadcastID, currentVersion, moduleInfo);
  };

  return remoteSettings;
}

var RemoteSettings = remoteSettingsFunction();

var remoteSettingsBroadcastHandler = {
  async receivedBroadcastMessage(data, broadcastID) {
    return RemoteSettings.pollChanges({ expectedTimestamp: data });
  },
};
