/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/chrome-worker */

"use strict";

/**
 * A worker dedicated to Remote Settings.
 */

importScripts("resource://gre/modules/workers/require.js",
              "resource://gre/modules/CanonicalJSON.jsm",
              "resource://gre/modules/third_party/jsesc/jsesc.js");

const IDB_NAME = "remote-settings";
const IDB_VERSION = 1;
const IDB_RECORDS_STORE = "records";
const IDB_TIMESTAMPS_STORE = "timestamps";

const Agent = {
  /**
   * Return the canonical JSON serialization of the changes
   * applied to the local records.
   * It has to match what is done on the server (See Kinto/kinto-signer).
   *
   * @param {Array<Object>} localRecords
   * @param {Array<Object>} remoteRecords
   * @param {String} timestamp
   * @returns {String}
   */
  async canonicalStringify(localRecords, remoteRecords, timestamp) {
    // Sort list by record id.
    let allRecords = localRecords.concat(remoteRecords).sort((a, b) => {
      if (a.id < b.id) {
        return -1;
      }
      return a.id > b.id ? 1 : 0;
    });
    // All existing records are replaced by the version from the server
    // and deleted records are removed.
    for (let i = 0; i < allRecords.length; /* no increment! */) {
      const rec = allRecords[i];
      const next = allRecords[i + 1];
      if ((next && rec.id == next.id) || rec.deleted) {
        allRecords.splice(i, 1); // remove local record
      } else {
        i++;
      }
    }
    const toSerialize = {
      last_modified: "" + timestamp,
      data: allRecords,
    };
    return CanonicalJSON.stringify(toSerialize, jsesc);
  },

  /**
   * If present, import the JSON file into the Remote Settings IndexedDB
   * for the specified bucket and collection.
   * (eg. blocklists/certificates, main/onboarding)
   * @param {String} bucket
   * @param {String} collection
   */
  async importJSONDump(bucket, collection) {
    const { data: records } = await loadJSONDump(bucket, collection);
    if (records.length > 0) {
      await importDumpIDB(bucket, collection, records);
    }
    return records.length;
  },
};

/**
 * Wrap worker invocations in order to return the `callbackId` along
 * the result. This will allow to transform the worker invocations
 * into promises in `RemoteSettingsWorker.jsm`.
 */
self.onmessage = (event) => {
  const { callbackId, method, args = [] } = event.data;
  Agent[method](...args)
    .then((result) => {
      self.postMessage({ callbackId, result });
    })
    .catch(error => {
      console.log(`RemoteSettingsWorker error: ${error}`);
      self.postMessage({ callbackId, error: "" + error });
    });
};

/**
 * Load (from disk) the JSON file distributed with the release for this collection.
 * @param {String}  bucket
 * @param {String}  collection
 */
async function loadJSONDump(bucket, collection) {
  const fileURI = `resource://app/defaults/settings/${bucket}/${collection}.json`;
  let response;
  try {
    response = await fetch(fileURI);
  } catch (e) {
    // Return empty dataset if file is missing.
    return { data: [] };
  }
  // Will throw if JSON is invalid.
  return response.json();
}

/**
 * Import the records into the Remote Settings Chrome IndexedDB.
 *
 * Note: This duplicates some logics from `kinto-offline-client.js`.
 *
 * @param {String} bucket
 * @param {String} collection
 * @param {Array<Object>} records
 */
async function importDumpIDB(bucket, collection, records) {
  // Open the DB. It will exist since if we are running this, it means
  // we already tried to read the timestamp in `remote-settings.js`
  const db = await openIDB(IDB_NAME, IDB_VERSION);

  // Each entry of the dump will be stored in the records store.
  // They are indexed by `_cid`, and their status is `synced`.
  const cid = bucket + "/" + collection;
  await executeIDB(db, IDB_RECORDS_STORE, store => {
    // Chain the put operations together, the last one will be waited by
    // the `transaction.oncomplete` callback.
    let i = 0;
    putNext();

    function putNext() {
      if (i == records.length) {
        return;
      }
      const entry = { ...records[i], _status: "synced", _cid: cid };
      store.put(entry).onsuccess = putNext; // On error, `transaction.onerror` is called.
      ++i;
    }
  });

  // Store the highest timestamp as the collection timestamp.
  const timestamp = Math.max(...records.map(record => record.last_modified));
  await executeIDB(db, IDB_TIMESTAMPS_STORE, store => store.put({ cid, value: timestamp }));
}

/**
 * Helper to wrap indexedDB.open() into a promise.
 */
async function openIDB(dbname, version) {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(dbname, version);
    request.onupgradeneeded = () => {
      // We should never have to initialize the DB here.
      reject(new Error(`Error accessing ${dbname} Chrome IDB at version ${version}`));
    };
    request.onerror = event => reject(event.target.error);
    request.onsuccess = event => {
      const db = event.target.result;
      resolve(db);
    };
  });
}

/**
 * Helper to wrap some IDBObjectStore operations into a promise.
 *
 * @param {IDBDatabase} db
 * @param {String} storeName
 * @param {function} callback
 */
async function executeIDB(db, storeName, callback) {
  const mode = "readwrite";
  return new Promise((resolve, reject) => {
    const transaction = db.transaction([storeName], mode);
    const store = transaction.objectStore(storeName);
    let result;
    try {
      result = callback(store);
    } catch (e) {
      transaction.abort();
      reject(e);
    }
    transaction.onerror = event => reject(event.target.error);
    transaction.oncomplete = event => resolve(result);
  });
}
