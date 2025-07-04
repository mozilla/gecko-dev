/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

/**
 * @typedef {import("./Utils.sys.mjs").ProgressAndStatusCallbackParams} ProgressAndStatusCallbackParams
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  Progress: "chrome://global/content/ml/Utils.sys.mjs",
  OPFS: "chrome://global/content/ml/OPFS.sys.mjs",
  URLChecker: "chrome://global/content/ml/Utils.sys.mjs",
  createFileUrl: "chrome://global/content/ml/Utils.sys.mjs",
  DEFAULT_ENGINE_ID: "chrome://global/content/ml/EngineProcess.sys.mjs",
  FILE_REGEX: "chrome://global/content/ml/EngineProcess.sys.mjs",
  isPrivateBrowsing: "chrome://global/content/ml/Utils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.ml.logLevel",
    prefix: "ML:ModelHub",
  });
});

const ALLOWED_HEADERS_KEYS = [
  "Content-Type",
  "ETag",
  "status",
  "fileSize", // the size in bytes we store
  "Content-Length", // the size we download (can be different when gzipped)
  "lastUpdated",
  "lastUsed",
];

const MOZILLA_HUB_HOSTNAME = "model-hub.mozilla.org";
const HF_HUB_HOSTNAME = "huggingface.co";
const MOCHITESTS_HOSTNAME = "mochitests";
const DEFAULT_CONTENT_TYPE = "application/octet-stream";
const DEFAULT_DELETE_TIMEOUT_MS = 5000;
const LOCAL_CHROME_PREFIX = "chrome://";

// Default indexedDB revision.
const DEFAULT_MODEL_REVISION = 6;

// The origin to use for storage. If null uses system.
const DEFAULT_PRINCIPAL_ORIGIN = null;

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "DEFAULT_MAX_CACHE_SIZE",
  "browser.ml.modelCacheMaxSize"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "DEFAULT_URL_TEMPLATE",
  "browser.ml.modelHubUrlTemplate",
  "{model}/{revision}"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "DEFAULT_ROOT_URL",
  "browser.ml.modelHubRootUrl",
  "https://model-hub.mozilla.org/"
);

const ONE_GIB = 1024 * 1024 * 1024;
const NO_ETAG = "NO_ETAG";

/**
 * Custom error when a fetch is attempted on a forbidden URL
 */
class ForbiddenURLError extends Error {
  constructor(url, rejectionType) {
    super(`Forbidden URL: ${url} (${rejectionType})`);
    this.name = "ForbiddenURLError";
    this.url = url;
  }
}

/**
 * Class representing a model owner.
 *
 * A model owner can be a user or an organization
 */
class ModelOwner {
  constructor({ hostname, owner }) {
    this.hostname = hostname;
    this.owner = owner;
  }

  /**
   * @type {string} model - The fully qualified model name (hub/owner/name)
   */
  static fromModel(model) {
    const hostname = model.split("/")[0];
    const owner = model.split("/")[1];
    return new ModelOwner({ hostname, owner });
  }

  /**
   * Gets the icon OPFS path
   *
   * @returns {Promise<void>}
   */
  #getIconFilePath() {
    return `modelOwners/${this.hostname}/${this.owner}/icon`;
  }

  /**
   * Removes ant cache associated with this owner
   *
   */
  async pruneCache() {
    const filePath = this.#getIconFilePath();
    try {
      const fileHandle = await lazy.OPFS.getFileHandle(filePath);

      if (fileHandle) {
        await lazy.OPFS.remove(filePath);
      }
    } catch (e) {
      // we can ignore this error, as the deleteIconFile may be called multiple times on the same file.
    }
  }

  /**
   * Returns the owner's icon
   *
   * @returns {Promise<string|null>}
   */
  async getIcon() {
    // If it's not the known HF hub root, we don't know how to fetch an icon
    if (
      ![MOCHITESTS_HOSTNAME, HF_HUB_HOSTNAME, MOZILLA_HUB_HOSTNAME].includes(
        this.hostname
      )
    ) {
      lazy.console.debug(
        "We don't know how to get icons from that hub",
        this.hostname
      );
      return null;
    }

    // Switch from Mozilla to Hugging Face if needed
    if (this.hostname === MOZILLA_HUB_HOSTNAME) {
      this.hostname = "huggingface.co";
    }

    const hubRootUrl = `https://${this.hostname}/`;
    const filePath = this.#getIconFilePath();
    let possibleUrls;

    if (this.hostname === MOCHITESTS_HOSTNAME) {
      possibleUrls = ["chrome://global/content/ml/mozilla-logo.webp"];
    } else {
      // Attempt to fetch (org first, then user, then default)
      possibleUrls = [
        `${hubRootUrl}api/organizations/${this.owner}/avatar?redirect=true`,
        `${hubRootUrl}api/users/${this.owner}/avatar?redirect=true`,
        "chrome://global/content/ml/mozilla-logo.webp",
      ];
    }
    const opfsFile = new lazy.OPFS.File({
      urls: possibleUrls,
      localPath: filePath,
    });
    return opfsFile.getAsObjectURL();
  }
}

/**
 * Class for managing a cache stored in IndexedDB.
 *
 */
class IndexedDBCache {
  /**
   * Reference to the IndexedDB database.
   *
   * @type {IDBDatabase|null}
   */
  db = null;

  /**
   * Reference to the IndexedDB principal.
   *
   * @type {Ci.nsIPrincipal|null}
   */
  #principal = null;

  /**
   * Version of the database. Null if not set.
   *
   * @type {number|null}
   */
  dbVersion = null;

  /**
   * Name of the database used by IndexedDB.
   *
   * @type {string}
   */
  dbName;

  /**
   * Name of the object store for storing files. (Retained for deletion in previous versions.)
   * Files are expected to be unique for a given tuple of file name, model, file, revision
   *
   * @type {string}
   */
  fileStoreName;

  /**
   * Name of the object store for storing headers.
   * Headers are expected to be unique for a given triplet model, file, revision
   *
   * @type {string}
   */
  headersStoreName;

  /**
   * Name of the object store for storing task names.
   * Tasks are expected to be unique for a given tuple of task name, model, file, revision
   *
   * @type {string}
   */
  taskStoreName;

  /**
   * Name of the object store for storing (model, file, revision, engineIds)
   *
   * @type {string}
   */
  enginesStoreName;

  /**
   * Name and KeyPath for indices to be created on object stores.
   *
   * @type {object}
   */
  #indices = {
    modelRevisionIndex: {
      name: "modelRevisionIndex",
      keyPath: ["model", "revision"],
    },
    modelRevisionFileIndex: {
      name: "modelRevisionFileIndex",
      keyPath: ["model", "revision", "file"],
    },
    taskModelRevisionIndex: {
      name: "taskModelRevisionIndex",
      keyPath: ["taskName", "model", "revision"],
    },
  };
  /**
   * Maximum size of the cache in GiB. Defaults to "browser.ml.modelCacheMaxSize".
   *
   * @type {number}
   */
  #maxSize = lazy.DEFAULT_MAX_CACHE_SIZE;

  /**
   * Private constructor to prevent direct instantiation.
   * Use IndexedDBCache.init to create an instance.
   *
   * @param {object} config
   * @param {string} config.dbName - The name of the database file.
   * @param {number} config.version - The version number of the database.
   * @param {number} config.maxSize Maximum size of the cache in GiB. Defaults to "browser.ml.modelCacheMaxSize".
   * @param {principal} config.principal - The principal to use for the database.
   */
  constructor({
    dbName = "modelFiles",
    version = DEFAULT_MODEL_REVISION,
    maxSize = lazy.DEFAULT_MAX_CACHE_SIZE,
    principal,
  } = {}) {
    this.dbName = dbName;
    this.dbVersion = version;
    this.fileStoreName = "files";
    this.headersStoreName = "headers";
    this.taskStoreName = "tasks";
    this.enginesStoreName = "engines";
    this.#maxSize = maxSize;
    this.#principal = principal;
  }

  /**
   * Delete a database and wait for it to close.
   */
  static async deleteDatabaseAndWait(
    principal,
    dbName,
    timeoutMs = DEFAULT_DELETE_TIMEOUT_MS
  ) {
    try {
      await lazy.OPFS.remove(dbName, { recursive: true });
    } catch (e) {
      // can be empty
    }

    return new Promise((resolve, reject) => {
      const request = indexedDB.deleteForPrincipal(principal, dbName);

      const timer = lazy.setTimeout(() => {
        reject(new Error("Request timed out (possibly blocked forever)"));
      }, timeoutMs);

      request.onsuccess = () => {
        lazy.clearTimeout(timer);
        resolve({ status: "success" });
      };

      request.onerror = () => {
        lazy.clearTimeout(timer);
        lazy.console.warn("Request error:", request.error);
        resolve({ status: "error", error: request.error });
      };

      request.onblocked = () => {
        lazy.console.warn(
          "Request blocked — waiting for other connections to close"
        );
        // Let it continue to wait
      };
    });
  }

  /**
   * Static method to create and initialize an instance of IndexedDBCache.
   *
   * @param {object} config
   * @param {string} [config.dbName="modelFiles"] - The name of the database.
   * @param {number} [config.version] - The version number of the database.
   * @param {number} config.maxSize Maximum size of the cache in bytes. Defaults to "browser.ml.modelCacheMaxSize".
   * @param {boolean} [config.reset=false] - Whether to reset the database.
   * @returns {Promise<IndexedDBCache>} An initialized instance of IndexedDBCache.
   */
  static async init({
    dbName = "modelFiles",
    version = DEFAULT_MODEL_REVISION,
    maxSize = lazy.DEFAULT_MAX_CACHE_SIZE,
    reset = false,
  } = {}) {
    const principal = DEFAULT_PRINCIPAL_ORIGIN
      ? Services.scriptSecurityManager.createContentPrincipalFromOrigin(
          DEFAULT_PRINCIPAL_ORIGIN
        )
      : Services.scriptSecurityManager.getSystemPrincipal();

    if (reset) {
      await IndexedDBCache.deleteDatabaseAndWait(principal, dbName);
    }

    const cacheInstance = new IndexedDBCache({
      dbName,
      version,
      maxSize,
      principal,
    });
    cacheInstance.db = await cacheInstance.#openDB();

    return cacheInstance;
  }

  /**
   * Called to close the DB connection and dispose the instance
   *
   */
  async dispose() {
    if (this.db) {
      this.db.close();
      this.db = null;
    }
  }

  #migrateStore(db, oldVersion) {
    const newVersion = db.version;
    lazy.console.debug(`Migrating from version ${oldVersion} to ${newVersion}`);
    try {
      // If we are migrating from version 5 to 6, we can skip the migration
      // as we just added the header lastUsed and lastUpdated fields
      if (oldVersion === 5 && newVersion === 6) {
        return;
      }

      if (oldVersion < newVersion) {
        for (const name of [
          this.fileStoreName,
          this.headersStoreName,
          this.taskStoreName,
          this.enginesStoreName,
        ]) {
          if (db.objectStoreNames.contains(name)) {
            db.deleteObjectStore(name);
          }
        }
      }
    } finally {
      lazy.console.debug("Migration done");
    }
  }

  #createOrMigrateIndices({ store, name, keyPath }) {
    if (store.indexNames.contains(name)) {
      if (!lazy.ObjectUtils.deepEqual(store.index(name).keyPath, keyPath)) {
        lazy.console.debug(
          `Deleting and recreating index ${name} on store ${store.name}`
        );
        store.deleteIndex(name);
        store.createIndex(name, keyPath);
      }
    } else {
      // index does not exist, so create
      lazy.console.debug(`Creating index ${name} on store ${store.name}`);
      store.createIndex(name, keyPath);
    }
  }

  /**
   * Enable persistence for a principal.
   *
   * @returns {Promise<boolean>} Wether persistence was successfully enabled.
   */

  async #ensurePersistentStorage() {
    try {
      const { promise, resolve, reject } = Promise.withResolvers();
      const request = Services.qms.persist(this.#principal);

      request.callback = () => {
        if (request.resultCode === Cr.NS_OK) {
          resolve();
        } else {
          reject(
            new Error(
              `Failed to persist storage for principal: ${this.#principal.originNoSuffix}`
            )
          );
        }
      };

      await promise;
      return true;
    } catch (error) {
      lazy.console.error("An unexpected error occurred:", error);
      return false;
    }
  }

  /**
   * Opens or creates the IndexedDB database.
   *
   * @returns {Promise<IDBDatabase>}
   */
  async #openDB() {
    let wasUpgraded = false;

    return new Promise((resolve, reject) => {
      if (DEFAULT_PRINCIPAL_ORIGIN) {
        this.#ensurePersistentStorage();
      }

      const request = indexedDB.openForPrincipal(
        this.#principal,
        this.dbName,
        this.dbVersion
      );

      request.onerror = event => reject(event.target.error);

      request.onupgradeneeded = event => {
        const db = event.target.result;
        const transaction = event.target.transaction;

        try {
          this.#migrateStore(db, event.oldVersion, transaction);

          if (!db.objectStoreNames.contains(this.headersStoreName)) {
            db.createObjectStore(this.headersStoreName, {
              keyPath: ["model", "revision", "file"],
            });
          }
          if (!db.objectStoreNames.contains(this.taskStoreName)) {
            db.createObjectStore(this.taskStoreName, {
              keyPath: ["taskName", "model", "revision", "file"],
            });
          }
          if (!db.objectStoreNames.contains(this.enginesStoreName)) {
            db.createObjectStore(this.enginesStoreName, {
              keyPath: ["model", "revision", "file"],
            });
          }

          const headerStore = transaction.objectStore(this.headersStoreName);
          this.#createOrMigrateIndices({
            store: headerStore,
            name: this.#indices.modelRevisionIndex.name,
            keyPath: this.#indices.modelRevisionIndex.keyPath,
          });

          const enginesStore = transaction.objectStore(this.enginesStoreName);
          this.#createOrMigrateIndices({
            store: enginesStore,
            name: this.#indices.modelRevisionIndex.name,
            keyPath: this.#indices.modelRevisionIndex.keyPath,
          });

          const taskStore = transaction.objectStore(this.taskStoreName);
          for (const { name, keyPath } of Object.values(this.#indices)) {
            this.#createOrMigrateIndices({ store: taskStore, name, keyPath });
          }

          wasUpgraded = true;
        } catch (error) {
          console.error("Migration failed:", error);
          reject(error);
        }
      };

      request.onsuccess = event => {
        const db = event.target.result;
        db.onversionchange = () => {
          lazy.console.debug(
            "The version of this database is changing. Closing."
          );
          db.close();
        };

        resolve(db); // Immediately resolve after DB is ready
      };
    }).then(async db => {
      if (wasUpgraded) {
        lazy.console.debug("Clearing OPFS cache");
        await lazy.OPFS.remove("modelFiles", {
          recursive: true,
          ignoreErrors: true,
        });
      }
      return db;
    });
  }

  /**
   * Generic method to get the data from a specified object store.
   *
   * @param {object} config
   * @param {string} config.storeName - The name of the object store.
   * @param {string} config.key - The key within the object store to retrieve the data from.
   * @param {?string} config.indexName - The store index to use.
   * @returns {Promise<Array<any>>}
   */
  async #getData({ storeName, key, indexName }) {
    return new Promise((resolve, reject) => {
      const transaction = this.db.transaction([storeName], "readonly");
      const store = transaction.objectStore(storeName);
      const request = (indexName ? store.index(indexName) : store).getAll(key);
      request.onerror = event => reject(event.target.error);
      request.onsuccess = event => resolve(event.target.result);
    });
  }

  /**
   * Generic method to get the unique keys from a specified object store.
   *
   * @param {object} config
   * @param {string} config.storeName - The name of the object store.
   * @param {string} config.key - The key within the object store to retrieve the data from.
   * @param {?string} config.indexName - The store index to use.
   * @param {?function(IDBCursor):boolean} config.filterFn - A function to execute for each key found.
   *  It should return a truthy value to keep the key, and a falsy value otherwise.
   *
   * @returns {Promise<Array<{primaryKey:any, key:any}>>}
   */
  async #getKeys({ storeName, key, indexName, filterFn }) {
    const keys = [];
    return new Promise((resolve, reject) => {
      const transaction = this.db.transaction([storeName], "readonly");
      const store = transaction.objectStore(storeName);
      const request = (
        indexName ? store.index(indexName) : store
      ).openKeyCursor(key, "nextunique");
      request.onerror = event => reject(event.target.error);
      request.onsuccess = event => {
        const cursor = event.target.result;

        if (cursor) {
          if (!filterFn || filterFn(cursor)) {
            keys.push({ primaryKey: cursor.primaryKey, key: cursor.key });
          }

          cursor.continue();
        } else {
          resolve(keys);
        }
      };
    });
  }

  /**
   * Generic method to check if data exists from a specified object store.
   *
   * @param {object} config
   * @param {string} config.storeName - The name of the object store.
   * @param {string} config.key - The key within the object store to retrieve the data from.
   * @param {?string} config.indexName - The store index to use.
   * @returns {Promise<boolean>} A promise that resolves with `true` if the key exists, otherwise `false`.
   */
  async #hasData({ storeName, key, indexName }) {
    return new Promise((resolve, reject) => {
      const transaction = this.db.transaction([storeName], "readonly");
      const store = transaction.objectStore(storeName);
      const request = (indexName ? store.index(indexName) : store).getKey(key);
      request.onerror = event => reject(event.target.error);
      request.onsuccess = event => resolve(event.target.result !== undefined);
    });
  }

  // Used in tests
  async _testGetData(storeName, key) {
    return (await this.#getData({ storeName, key }))[0];
  }

  /**
   * Generic method to update data in a specified object store.
   *
   * @param {string} storeName - The name of the object store.
   * @param {object} data - The data to store.
   * @returns {Promise<void>}
   */
  async #updateData(storeName, data) {
    return new Promise((resolve, reject) => {
      const transaction = this.db.transaction([storeName], "readwrite");
      const store = transaction.objectStore(storeName);
      const request = store.put(data);
      request.onerror = event => reject(event.target.error);
      request.onsuccess = () => resolve();
    });
  }

  /**
   * Deletes a specific cache entry.
   *
   * @param {string} storeName - The name of the object store.
   * @param {string} key - The key of the entry to delete.
   * @returns {Promise<void>}
   */
  async #deleteData(storeName, key) {
    return new Promise((resolve, reject) => {
      const transaction = this.db.transaction([storeName], "readwrite");
      const store = transaction.objectStore(storeName);
      const request = store.delete(key);
      request.onerror = event => reject(event.target.error);
      request.onsuccess = () => resolve();
    });
  }

  /**
   * Generates an IndexedDB query to retrieve entries from the appropriate index.
   *
   * @param {object} config - Configuration object.
   * @param {?string} config.taskName - The name of the inference task. Retrieves all tasks if null.
   * @param {?string} config.model - The model name (organization/name). Retrieves all models if null.
   * @param {?string} config.revision - The model revision. Retrieves all revisions if null.
   *
   * @returns {object} queryIndex - The query and index for retrieving entries.
   * @returns {?string} queryIndex.query - The query.
   * @returns {?string} queryIndex.indexName - The index name.
   */
  #getFileQuery({ taskName, model, revision }) {
    // See https://developer.mozilla.org/en-US/docs/Web/API/IDBKeyRange for explanation on the query.

    // Case 1: Query to retrieve all entries matching taskName, model, and revision
    if (taskName && model && revision) {
      return {
        key: [taskName, model, revision],
        indexName: this.#indices.taskModelRevisionIndex.name,
      };
    }
    // Case 2: Query to retrieve all entries with taskName
    if (taskName && !model && !revision) {
      return {
        key: IDBKeyRange.bound([taskName], [taskName, []]),
        indexName: this.#indices.taskModelRevisionIndex.name,
      };
    }
    // Case 3: Query to retrieve all entries matching model and revision
    if (!taskName && model && revision) {
      return {
        key: [model, revision],
        indexName: this.#indices.modelRevisionIndex.name,
      };
    }
    // Case 4: Query to retrieve all entries
    if (!taskName && !model && !revision) {
      return { key: null, indexName: null };
    }
    // Case 5: Query to retrieve all entries matching taskName and model
    if (taskName && model && !revision) {
      return {
        key: IDBKeyRange.bound([taskName, model], [taskName, model, []]),
        indexName: this.#indices.taskModelRevisionIndex.name,
      };
    }
    throw new Error("Invalid query configuration.");
  }

  /**
   * Checks if a specified model file exists in storage.
   *
   * @param {object} config
   * @param {string} config.model - The model name (organization/name)
   * @param {string} config.revision - The model revision.
   * @param {string} config.file - The file name.
   * @returns {Promise<boolean>} A promise that resolves with `true` if the key exists, otherwise `false`.
   */
  async fileExists({ model, revision, file }) {
    // First, check if the file is in the headers store
    const fileMedataExists = this.#hasData({
      storeName: this.headersStoreName,
      key: [model, revision, file],
    });

    if (!fileMedataExists) {
      return false;
    }

    // Now check if we have the file in OPFS
    const localFilePath = this.generateFilePathInOPFS({
      model,
      revision,
      file,
    });

    lazy.console.debug(
      "ModelHub: Checking if file exists in OPFS: " + localFilePath
    );

    try {
      const fileHandle = await lazy.OPFS.getFileHandle(localFilePath);

      if (!fileHandle) {
        // The file is not in OPFS and is in IndexDB...
        // TODO: we should clean up
        lazy.console.debug(
          "ModelHub: The file is not in OPFS and is in IndexDB..."
        );
        return false;
      }
    } catch (e) {
      return false;
    }
    return true;
  }

  /**
   * Generate the path where a model file will be stored in Origin Private FileSystem (OPFS).
   *
   * @param {object} config
   * @param {string} config.model - The model name (organization/name)
   * @param {string} config.revision - The model revision.
   * @param {string} config.file - The file name.
   * @returns {string} The generated file path.
   */
  generateFilePathInOPFS({ model, revision, file }) {
    return `${this.dbName}/${model}/${revision}/${file}`;
  }

  /**
   * Retrieves the headers for a specific cache entry.
   *
   * @param {object} config
   * @param {string} config.model - The model name (organization/name)
   * @param {string} config.revision - The model revision.
   * @param {string} config.file - The file name.
   * @returns {Promise<object|null>} The headers or null if not found.
   */
  async getHeaders({ model, revision, file }) {
    return (
      await this.#getData({
        storeName: this.headersStoreName,
        key: [model, revision, file],
      })
    )[0]?.headers;
  }

  /**
   * Sets the headers for a specific cache entry.
   *
   * @param {object} config
   * @param {string} config.model - The model name (organization/name)
   * @param {string} config.revision - The model revision.
   * @param {string} config.file - The file name.
   * @param {object} config.headers - The headers to set.
   * @returns {Promise<void>} A promise that resolves when the headers are set.
   */
  async setHeaders({ model, revision, file, headers }) {
    return await this.#updateData(this.headersStoreName, {
      model,
      revision,
      file,
      headers,
    });
  }

  /**
   * Retrieves the file for a specific cache entry.
   *
   * @param {object} config
   * @param {string} config.engineId - The engine Id. Defaults to "default-engine"
   * @param {string} config.model - The model name (organization/name)
   * @param {string} config.revision - The model revision.
   * @param {string} config.file - The file name.
   * @returns {Promise<[Blob, object]|null>} The file Blob and its headers or null if not found.
   */
  async getFile({ engineId = lazy.DEFAULT_ENGINE_ID, model, revision, file }) {
    const headers = await this.getHeaders({ model, revision, file });

    if (headers) {
      await this.#updateEngines({
        engineId,
        model,
        revision,
        file,
      });

      const fileData = await (
        await lazy.OPFS.getFileHandle(
          this.generateFilePathInOPFS({ model, revision, file })
        )
      ).getFile();

      // mark the last used header value
      headers.lastUsed = Date.now();
      await this.setHeaders({ model, revision, file, headers });

      return [fileData, headers];
    }
    return null; // Return null if no file is found
  }

  async #updateEngines({ engineId, model, revision, file }) {
    // Add the consumer id to the set of consumer ids
    const stored = await this.#getData({
      storeName: this.enginesStoreName,
      key: [model, revision, file],
    });

    let engineIds;

    if (stored.length) {
      engineIds = stored[0].engineIds || [];
      if (!engineIds.includes(engineId)) {
        engineIds.push(engineId);
      }
    } else {
      engineIds = [engineId];
    }

    await this.#updateData(this.enginesStoreName, {
      engineIds,
      model,
      revision,
      file,
    });
  }

  /**
   * Adds or updates task entry.
   *
   * @param {object} config
   * @param {string} config.engineId - ID of the engine
   * @param {string} config.taskName - name of the inference task.
   * @param {string} config.model - The model name (organization/name).
   * @param {string} config.revision - The model version.
   * @param {string} config.file - The file name.
   * @returns {Promise<void>}
   */
  async updateTask({
    engineId = lazy.DEFAULT_ENGINE_ID,
    taskName,
    model,
    revision,
    file,
  }) {
    await this.#updateEngines({
      engineId,
      model,
      revision,
      file,
    });

    await this.#updateData(this.taskStoreName, {
      taskName,
      model,
      revision,
      file,
    });
  }

  /**
   * Estimate the disk size in bytes for a domain origin. If no origin is provided, assume the system origin.
   *
   * @param {?string} origin - The origin.
   * @returns {Promise<number>} The estimated size.
   */
  async #estimateUsageForOrigin(origin) {
    const { promise, resolve, reject } = Promise.withResolvers();
    try {
      const principal = origin
        ? Services.scriptSecurityManager.createContentPrincipalFromOrigin(
            origin
          )
        : Services.scriptSecurityManager.getSystemPrincipal();
      Services.qms.getUsageForPrincipal(principal, request => {
        if (request.resultCode == Cr.NS_OK) {
          resolve(request.result.usage);
        } else {
          reject(new Error(request.resultCode));
        }
      });
    } catch (error) {
      reject(new Error(`An unexpected error occurred: ${error.message}`));
    }

    return promise;
  }

  /**
   * Adds or updates a cache entry.
   *
   * @param {object} config
   * @param {string} config.engineId - ID of the engine.
   * @param {string} config.taskName - name of the inference task.
   * @param {string} config.model - The model name (organization/name).
   * @param {string} config.revision - The model version.
   * @param {string} config.file - The file name.
   * @param {Blob | string} config.data - The content or path to the data to cache.
   * @param {object} [config.headers] - The headers for the file.
   * @returns {Promise<timestamp>} A promise that resolves when the cache entry is added or updated.
   */
  async put({
    engineId = lazy.DEFAULT_ENGINE_ID,
    taskName,
    model,
    revision,
    file,
    data,
    headers,
  }) {
    const updatePromises = [];
    const fileSize = headers?.fileSize ?? data.size;
    const cacheKey = this.generateFilePathInOPFS({ model, revision, file });
    const totalSize = await this.#estimateUsageForOrigin(
      DEFAULT_PRINCIPAL_ORIGIN
    );

    if (totalSize + fileSize > this.#maxSize * ONE_GIB) {
      throw new Error(`Exceeding cache size limit of ${this.#maxSize}GiB"`);
    }

    // Store the file data if a blob is passed.
    if (Blob.isInstance(data) && !File.isInstance(data)) {
      updatePromises.push(
        data.stream().pipeTo(
          await lazy.OPFS.getFileHandle(cacheKey, {
            create: true,
          }).then(handle =>
            handle.createWritable({
              keepExistingData: false,
              mode: "siloed",
            })
          )
        )
      );
    }

    // Store task metadata
    updatePromises.push(
      this.updateTask({
        engineId,
        taskName,
        model,
        revision,
        file,
      })
    );

    const currentTimeSinceEpoch = Date.now();

    // Update headers store - whith defaults for ETag and Content-Type
    headers = headers || {};
    headers["Content-Type"] = headers["Content-Type"] ?? DEFAULT_CONTENT_TYPE;
    headers.fileSize = fileSize;
    headers.ETag = headers.ETag ?? NO_ETAG;
    headers.lastUpdated = currentTimeSinceEpoch;
    headers.lastUsed = currentTimeSinceEpoch;

    // filter out any keys that are not allowed
    headers = Object.keys(headers)
      .filter(key => ALLOWED_HEADERS_KEYS.includes(key))
      .reduce((obj, key) => {
        obj[key] = headers[key];
        return obj;
      }, {});

    lazy.console.debug(`Storing ${cacheKey} with headers:`, headers);

    // Update headers
    updatePromises.push(
      this.#updateData(this.headersStoreName, {
        model,
        revision,
        file,
        headers,
      })
    );

    await Promise.all(updatePromises);
    return currentTimeSinceEpoch;
  }

  /**
   * Deletes files associated with a specific engine ID.
   * If the engine ID is the only one associated with a file, the file is deleted.
   * Otherwise, the engine ID is removed from the file's engine list.
   *
   * @async
   *
   * @param {object} config
   * @param {?string} config.engineId - The ID of the engine whose files are to be deleted.
   * @param {?string} config.deletedBy - The feature who deleted the model
   * @returns {Promise<void>} A promise that resolves once the deletion process is complete.
   */
  async deleteFilesByEngine({ engineId, deletedBy = "other" }) {
    // looking at all files for deletion candidates
    const files = [];
    const uniqueModelRevisions = [];
    const items = await this.#getData({ storeName: this.enginesStoreName });

    for (const item of items) {
      if (item.engineIds.includes(engineId)) {
        // if it's the only one, we delete the file
        if (item.engineIds.length === 1) {
          files.push({
            model: item.model,
            file: item.file,
            revision: item.revision,
          });
        } else {
          // we remove the entry
          const engineIds = new Set(item.engineIds);
          engineIds.delete(engineId);

          await this.#updateData(this.enginesStoreName, {
            engineIds: Array.from(engineIds),
            model: item.model,
            revision: item.revision,
            file: item.file,
          });
        }

        // Track unique (model, revision) pairs
        if (
          !uniqueModelRevisions.some(
            ([m, r]) => m === item.model && r === item.revision
          )
        ) {
          uniqueModelRevisions.push([item.model, item.revision]);
        }
      }
    }

    // deleting the files from task, engines, files, headers
    for (const file of files) {
      await this.#deleteFile(file);
    }

    // send metrics events
    for (const [model, revision] of uniqueModelRevisions) {
      Glean.firefoxAiRuntime.modelDeletion.record({
        modelId: model,
        modelRevision: revision,
        deletedBy,
      });
    }
  }

  /**
   * Deletes a file and its associated data across various storage locations.
   *
   * @async
   * @private
   * @param {object} file - The file object containing model, revision, and file details.
   * @param {string} file.model - The model associated with the file.
   * @param {string} file.revision - The revision of the file.
   * @param {string} file.file - The filename or unique identifier for the file.
   * @returns {Promise<void>} A promise that resolves once the file and associated data are deleted.
   */
  async #deleteFile({ model, revision, file }) {
    const owner = ModelOwner.fromModel(model);

    await Promise.all([
      // For now we delete the icon file any time a file from a model is removed.
      owner.pruneCache(),
      this.#deleteData(this.headersStoreName, [model, revision, file]),
      lazy.OPFS.remove(this.generateFilePathInOPFS({ model, revision, file })),
    ]);
  }

  /**
   * Deletes all data related to the specifed models.
   *
   * @param {object} config
   *
   * @param {?string} config.model - The model name (organization/name) to delete.
   * @param {?string} config.revision - The model version to delete.
   *  If both model and revision are null, delete models of any name and version.
   *
   * @param {?string} config.taskName - name of the inference task to delete.
   *                                    If null, delete specified models for all tasks.
   *
   * @param {?function(IDBCursor):boolean} config.filterFn - A function to execute for each model file candidate for deletion.
   * It should return a truthy value to delete the model file, and a falsy value otherwise.
   *
   * @param {?string} config.deletedBy - The feature who deleted the model
   *
   * @throws {Error} If a revision is defined, the model must also be defined.
   *                 If the model is not defined, the revision should also not be defined.
   *                 Otherwise, an error will be thrown.

   * @returns {Promise<void>}
   */
  async deleteModels({ taskName, model, revision, filterFn, deletedBy }) {
    Glean.firefoxAiRuntime.modelDeletion.record({
      modelId: model,
      modelRevision: revision,
      deletedBy,
    });

    const tasks = await this.#getData({
      storeName: this.taskStoreName,
      ...this.#getFileQuery({ taskName, model, revision }),
    });

    if (!tasks.length) {
      lazy.console.debug("No models to delete found in task store", {
        taskName,
        model,
        revision,
      });
    }

    let deletePromises = [];
    const filesToMaybeDelete = new Set();
    for (const task of tasks) {
      if (
        filterFn &&
        !filterFn({
          taskName: task.taskName,
          model: task.model,
          revision: task.revision,
          file: task.file,
        })
      ) {
        continue;
      }
      filesToMaybeDelete.add(
        JSON.stringify([task.model, task.revision, task.file])
      );

      deletePromises.push(
        this.#deleteData(this.taskStoreName, [
          task.taskName,
          task.model,
          task.revision,
          task.file,
        ])
      );
    }
    await Promise.all(deletePromises);

    deletePromises = [];

    const remainingFileKeys = await this.#getKeys({
      storeName: this.taskStoreName,
      indexName: this.#indices.modelRevisionFileIndex.name,
    });

    const remainingFiles = new Set();

    for (const { key } of remainingFileKeys) {
      remainingFiles.add(JSON.stringify(key));
    }

    const filesToDelete = filesToMaybeDelete.difference(remainingFiles);

    for (const key of filesToDelete) {
      const [modelValue, revisionValue, fileValue] = JSON.parse(key);
      deletePromises.push(
        this.#deleteFile({
          model: modelValue,
          revision: revisionValue,
          file: fileValue,
        })
      );
    }
    await Promise.all(deletePromises);
    if (deletePromises.length) {
      lazy.console.debug(
        `Deleted model ${model} (${deletePromises.length} files.)`
      );
    }
  }

  /**
   * Lists all files for a given model and revision stored in the cache,
   * and aggregates metadata from the file headers.
   *
   * When a `taskName` is provided, the method retrieves all model/revision
   * pairs associated with that task; otherwise, it uses the provided `model`
   * and `revision`. It then queries the store to retrieve file information (path
   * and headers) and aggregates metadata (totalSize, lastUsed, updateDate, engineIds)
   * across all files.
   *
   * @param {object} config - The configuration for querying the files.
   * @param {?string} config.model - The model name (in "organization/name" format).
   * @param {?string} config.revision - The model version.
   * @param {?string} config.taskName - The name of the inference task.
   * @returns {Promise<{
   *   files: Array<{ path: string, headers: object, engineIds: Array<string> }>,
   *   metadata: { totalSize: number, lastUsed: number, updateDate: number, engineIds: Array<string> }
   * }>} An object containing:
   *   - files: an array of file records with their path, headers, and engine IDs.
   *   - metadata: aggregated metadata computed from all the files.
   */
  async listFiles({ taskName, model, revision }) {
    // When not providing taskName, both model and revision must be defined.
    if (!taskName && (!model || !revision)) {
      throw new Error("Both model and revision must be defined");
    }

    // Determine which model/revision pairs we want files for.
    let modelRevisions = [{ model, revision }];
    if (taskName) {
      // Get all model/revision pairs associated with this task.
      const keysData = await this.#getKeys({
        storeName: this.taskStoreName,
        ...this.#getFileQuery({ taskName, model, revision }),
      });
      modelRevisions = keysData.map(({ key }) => ({
        model: key[1],
        revision: key[2],
      }));
    }

    // For each model/revision, query for headers data.
    const fileDataPromises = modelRevisions.map(task =>
      this.#getData({
        storeName: this.headersStoreName,
        indexName: this.#indices.modelRevisionIndex.name,
        key: [task.model, task.revision],
      })
    );
    const fileData = (await Promise.all(fileDataPromises)).flat();

    // Initialize aggregated metadata.
    let totalFileSize = 0;
    let aggregatedLastUsed = 0;
    let aggregatedUpdateDate = 0;
    let aggregatedEngineIds = [];

    // Process each file entry.
    const files = [];
    for (const { file: path, headers } of fileData) {
      const stored = await this.#getData({
        storeName: this.enginesStoreName,
        key: [model, revision, path],
      });

      if (stored.length) {
        aggregatedEngineIds = stored[0].engineIds || [];
      }
      // Aggregate metadata.
      totalFileSize += headers.fileSize;
      aggregatedLastUsed = Math.max(aggregatedLastUsed, headers.lastUsed);
      aggregatedUpdateDate = Math.max(
        aggregatedUpdateDate,
        headers.lastUpdated
      );
      files.push({ path, headers, engineIds: headers.engineIds || [] });
    }

    return {
      files,
      metadata: {
        totalSize: totalFileSize,
        lastUsed: aggregatedLastUsed,
        updateDate: aggregatedUpdateDate,
        engineIds: aggregatedEngineIds,
      },
    };
  }

  /**
   * Lists all models stored in the cache.
   *
   * @returns {Promise<Array<{name: string, revision: string}>>}
   *          An array of model identifiers.
   */
  async listModels() {
    // Get all keys (model/revision pairs) from the underlying store.
    const modelRevisions = await this.#getKeys({
      storeName: this.taskStoreName,
      indexName: this.#indices.modelRevisionIndex.name,
    });

    const models = [];
    // Process each key entry.
    for (const { primaryKey } of modelRevisions) {
      const taskName = primaryKey[0];
      const model = primaryKey[1];
      const revision = primaryKey[2];

      models.push({
        taskName,
        name: model,
        revision,
      });
    }
    return models;
  }
}

//exporting for testing purposes only
export const TestIndexedDBCache = IndexedDBCache;

export class ModelHub {
  /**
   * Tracks whether the last download of a session was successful.
   *
   * @type {Map<string, boolean>}
   */
  #lastDownloadOk = new Map();

  /**
   * Create an instance of ModelHub.
   *
   * @param {object} config
   * @param {string} config.rootUrl - Root URL used to download models.
   * @param {string} config.urlTemplate - The template to retrieve the full URL using a model name and revision.
   * @param {Array<{filter: 'ALLOW'|'DENY', urlPrefix: string}>} config.allowDenyList - Array of URL patterns with filters.
   * @param {boolean} [config.reset=false] - Whether to reset the database.
   */
  constructor({
    rootUrl = lazy.DEFAULT_ROOT_URL,
    urlTemplate = lazy.DEFAULT_URL_TEMPLATE,
    allowDenyList = null,
    reset = false,
  } = {}) {
    this.rootUrl = rootUrl;
    this.cache = null;

    // Ensures the URL template is well-formed and does not contain any invalid characters.
    const pattern = /^(?:\{\w+\}|\w+)(?:\/(?:\{\w+\}|\w+))*$/;
    //               ^                                         $   Start and end of string
    //                (?:\{\w+\}|\w+)                            Match a {placeholder} or alphanumeric characters
    //                                 (?:\/(?:\$\{\w+\}|\w+))*    Zero or more groups of a forward slash followed by a ${placeholder} or alphanumeric characters
    if (!pattern.test(urlTemplate)) {
      throw new Error(`Invalid URL template: ${urlTemplate}`);
    }
    this.urlTemplate = urlTemplate;
    if (Services.env.exists("MOZ_ALLOW_EXTERNAL_ML_HUB")) {
      this.allowDenyList = null;
    } else {
      this.allowDenyList = new lazy.URLChecker(allowDenyList);
    }
    this.reset = reset;
  }

  async #initCache() {
    if (this.cache) {
      return;
    }
    this.cache = await IndexedDBCache.init({ reset: this.reset });
  }

  async #fetch(url, options) {
    const result = this.allowDenyList && this.allowDenyList.allowedURL(url);
    if (result && !result.allowed) {
      throw new ForbiddenURLError(url, result.rejectionType);
    }
    const response = await fetch(url, options);

    if (!response.ok) {
      throw new Error(
        `HTTP error! Status: ${response.status} ${response.statusText}`
      );
    }

    return response;
  }

  /**
   * This method takes a model URL and parses it to extract the
   * model name, optional model version, and file path.
   *
   * The expected URL format are :
   *
   * `/organization/model/revision/filePath`
   * `https://hub/organization/model/revision/filePath`
   *
   * @param {string} url - The full URL to the model, including protocol and domain - or the relative path.
   * @returns {object} An object containing the parsed components of the URL. The
   *                   object has properties `model`, `modelWithHostname` and `file`,
   *                   and optionally `revision` if the URL includes a version.
   * @throws {Error} Throws an error if the URL does not start with `this.rootUrl` or
   *                 if the URL format does not match the expected structure.
   *
   * @example
   * // For a URL
   * parseModelUrl("https://example.com/org1/model1/v1/file/path");
   * // returns { model: "org1/model1", modelWithHostname: "example.com/org1/model1", revision: "v1", file: "file/path" }
   *
   * @example
   * // For a relative URL
   * parseModelUrl("/org1/model1/revision/file/path");
   * // returns { model: "org1/model1", modelWithHostname: "example.com/org1/model1", revision: "v1", file: "file/path" }
   */
  parseUrl(url, options = {}) {
    let parts;
    const rootUrl = options.rootUrl || this.rootUrl;
    const urlTemplate =
      options.urlTemplate || this.urlTemplate || lazy.DEFAULT_URL_TEMPLATE;
    let hostname;

    // Check if the URL is relative or absolute
    if (url.startsWith("/")) {
      // relative URL
      parts = url.slice(1); // Remove leading slash
      hostname = new URL(rootUrl).hostname;
    } else {
      // absolute URL
      if (!url.startsWith(rootUrl)) {
        throw new Error(`Invalid domain for model URL: ${url}`);
      }
      const urlObject = new URL(url);
      hostname = urlObject.hostname;
      const rootUrlObject = new URL(rootUrl);

      // Remove the root URL's pathname from the full URL's pathname
      const relativePath = urlObject.pathname.substring(
        rootUrlObject.pathname.length
      );
      parts = relativePath.slice(1); // Remove leading slash
    }

    // Match the parts with the template
    const templateRegex = urlTemplate
      .replace("{model}", "(?<model>[^/]+/[^/]+)")
      .replace("{revision}", "(?<revision>[^/]+)");

    // Create a regex to match the structure
    const regex = new RegExp(`^${templateRegex}/(?<file>.+)$`);
    const match = parts.match(regex);

    if (!match) {
      throw new Error(`Invalid model URL format: ${url}`);
    }

    // Extract the matched parts
    const { model, revision, file } = match.groups;

    if (!file || !file.length) {
      throw new Error(`Invalid model URL: ${url}`);
    }

    const modelWithHostname = `${hostname}/${model}`;
    return {
      model,
      revision,
      file,
      modelWithHostname,
    };
  }

  /** Creates the file URL from the organization, model, and version.
   *
   * @param {object} config - The configuration object to be updated.
   * @param {string} config.model - model name
   * @param {string} config.revision - model revision
   * @param {string} config.file - filename
   * @param {string} config.modelHubRootUrl - root url of the model hub
   * @param {string} config.modelHubUrlTemplate - url template of the model hub
   * @returns {string} The full URL
   */
  #fileUrl({ model, revision, file, modelHubRootUrl, modelHubUrlTemplate }) {
    const rootUrl = modelHubRootUrl || this.rootUrl;
    return lazy.createFileUrl({
      model,
      revision,
      file,
      rootUrl,
      urlTemplate: modelHubUrlTemplate || this.urlTemplate,
      addDownloadParams: !rootUrl.startsWith(LOCAL_CHROME_PREFIX),
    });
  }

  /** Checks the model and revision inputs.
   *
   * @param { string } model
   * @param { string } revision
   * @param { string } file
   * @returns { Error } The error instance(can be null)
   */
  static checkInput(model, revision, file) {
    // Matches a string with the format 'organization/model' or just 'model' where:
    // - 'organization' consists only of letters, digits, and hyphens, cannot start or end with a hyphen,
    //   and cannot contain consecutive hyphens.
    // - 'model' can contain letters, digits, hyphens, underscores, or periods.
    //
    // Pattern breakdown:
    //   ^                                     Start of string
    //    (?:                                  non-capturing group to make 'organization/' optional
    //      (?!-)                              Negative lookahead for 'organization' not starting with hyphen
    //           (?!.*--)                      Negative lookahead for 'organization' not containing consecutive hyphens
    //                   [A-Za-z0-9-]+         'organization' part: Alphanumeric characters or hyphens
    //                              (?<!-)     Negative lookbehind for 'organization' not ending with a hyphen
    //                                    \/   Literal '/' character separating 'organization' and 'model'
    //    )?                                   make 'organization/' group optional
    //                                      [A-Za-z0-9-_.]+    'model' part: Alphanumeric characters, hyphens, underscores, or periods
    //                                                    $    End of string
    const modelRegex =
      /^(?:(?!-)(?!.*--)[A-Za-z0-9-]+(?<!-)\/)?[A-Za-z0-9-_.]+$/;

    // Matches strings consisting of alphanumeric characters, hyphens, or periods.
    //
    //                    ^               $   Start and end of string
    //                     [A-Za-z0-9-.]+     Alphanum characters, hyphens, or periods, one or more times
    const versionRegex = /^[A-Za-z0-9-.]+$/;

    if (typeof model !== "string" || !modelRegex.test(model)) {
      return new Error("Invalid model name.");
    }

    if (
      !versionRegex.test(revision) ||
      revision.includes(" ") ||
      /[\^$]/.test(revision)
    ) {
      return new Error("Invalid version identifier.");
    }

    if (!lazy.FILE_REGEX.test(file)) {
      return new Error(`Invalid file name ${file}`);
    }

    return null;
  }

  /**
   * Deletes all model files for the specified task and model, except for the specified revision.
   *
   * @param {object} config - Configuration object.
   * @param {string} config.taskName - The name of the inference task.
   * @param {string} config.modelWithHostname - The model name (hostname/organization/name).
   * @param {string} config.targetRevision - The revision to keep.
   *
   * @returns {Promise<void>}
   */
  async deleteNonMatchingModelRevisions({
    taskName,
    modelWithHostname,
    targetRevision,
  }) {
    // Ensure all required parameters are provided
    if (!taskName || !modelWithHostname || !targetRevision) {
      throw new Error(
        "taskName, modelWithHostname, and targetRevision are required."
      );
    }

    await this.#initCache();

    // Delete models with revisions that do not match the targetRevision
    return this.cache.deleteModels({
      taskName,
      model: modelWithHostname,
      filterFn: record => record.revision !== targetRevision,
    });
  }

  /**
   * Returns the ETag value given an URL
   *
   * @param {string} url
   * @param {number} timeout in ms. Default is 1000
   * @returns {Promise<string>} ETag (can be null)
   */
  async getETag(url, timeout = 1000) {
    const controller = new AbortController();
    const id = lazy.setTimeout(() => controller.abort(), timeout);

    try {
      const headResponse = await this.#fetch(url, {
        method: "HEAD",
        signal: controller.signal,
      });
      const currentEtag = headResponse.headers.get("ETag");
      return currentEtag;
    } catch (error) {
      if (error instanceof ForbiddenURLError) {
        throw error;
      }
      lazy.console.warn("An error occurred when calling HEAD:", error);
      return null;
    } finally {
      lazy.clearTimeout(id);
    }
  }

  /**
   * Given an organization, model, and version, fetch a model file in the hub as a Response.
   *
   * @param {object} config
   * @param {string} config.engineId - The model engine id
   * @param {string} config.taskName - name of the inference task.
   * @param {string} config.model - The model name (organization/name).
   * @param {string} config.revision - The model revision.
   * @param {string} config.file - The file name.
   * @param {string} config.modelHubRootUrl - root url of the model hub
   * @param {string} config.modelHubUrlTemplate - url template of the model hub
   * @returns {Promise<Response>} The file content
   */
  async getModelFileAsResponse({
    engineId,
    taskName,
    model,
    revision,
    file,
    modelHubRootUrl,
    modelHubUrlTemplate,
  }) {
    const [blob, headers] = await this.getModelFileAsBlob({
      engineId,
      taskName,
      model,
      revision,
      file,
      modelHubRootUrl,
      modelHubUrlTemplate,
    });

    return new Response(blob.stream(), { headers });
  }

  /**
   * Given an organization, model, and version, fetch a model file in the hub as an blob.
   *
   * @param {object} config
   * @param {string} config.engineId - The model engine id
   * @param {string} config.taskName - name of the inference task.
   * @param {string} config.model - The model name (organization/name).
   * @param {string} config.revision - The model revision.
   * @param {string} config.file - The file name.
   * @param {string} config.modelHubRootUrl - root url of the model hub
   * @param {string} config.modelHubUrlTemplate - url template of the model hub
   * @param {?function(ProgressAndStatusCallbackParams):void} config.progressCallback A function to call to indicate progress status.
   * @returns {Promise<[Blob, object]>} The file content
   */
  async getModelFileAsBlob({
    engineId,
    taskName,
    model,
    revision,
    file,
    modelHubRootUrl,
    modelHubUrlTemplate,
    progressCallback,
  }) {
    const [filePath, headers] = await this.getModelDataAsFile({
      engineId,
      taskName,
      model,
      revision,
      file,
      modelHubRootUrl,
      modelHubUrlTemplate,
      progressCallback,
    });

    const fileObject = await (
      await lazy.OPFS.getFileHandle(filePath)
    ).getFile();

    // A file is a blob, so we can return it directly.
    return [fileObject, headers];
  }

  /**
   * Given an organization, model, and version, fetch a model file in the hub as an ArrayBuffer
   * while supporting status callback.
   *
   * @param {object} config
   * @param {string} config.engineId - The model engine id
   * @param {string} config.taskName - name of the inference task.
   * @param {string} config.model - The model name (organization/name).
   * @param {string} config.revision - The model revision.
   * @param {string} config.file - The file name.
   * @param {string} config.modelHubRootUrl - root url of the model hub
   * @param {string} config.modelHubUrlTemplate - url template of the model hub
   * @param {?function(ProgressAndStatusCallbackParams):void} config.progressCallback A function to call to indicate progress status.
   * @returns {Promise<[ArrayBuffer, headers]>} The file content
   */
  async getModelFileAsArrayBuffer({
    engineId,
    taskName,
    model,
    revision,
    file,
    modelHubRootUrl,
    modelHubUrlTemplate,
    progressCallback,
  }) {
    let [blob, headers] = await this.getModelFileAsBlob({
      engineId,
      taskName,
      model,
      revision,
      file,
      modelHubRootUrl,
      modelHubUrlTemplate,
      progressCallback,
    });

    return [await blob.arrayBuffer(), headers];
  }

  extractHeaders(response) {
    return {
      // We don't store the boundary or the charset, just the content type,
      // so we drop what's after the semicolon.
      "Content-Type": (
        response.headers.get("Content-Type") || DEFAULT_CONTENT_TYPE
      )
        .split(";")[0]
        .trim(),
      "Content-Length": response.headers.get("Content-Length"),
      ETag: response.headers.get("ETag"),
    };
  }

  /**
   * Notify that a model download is complete.
   *
   * @param {object} config
   * @param {string} config.engineId - The engine id.
   * @param {string} config.model - The model name (organization/name).
   * @param {string} config.revision - The model revision.
   * @param {string} config.featureId - The engine id.
   * @param {string} config.sessionId - Shared across the same model download session.
   * @returns {Promise<[string, object]>} The file local path and headers
   */
  async notifyModelDownloadComplete({
    engineId,
    model,
    revision,
    featureId,
    sessionId,
  }) {
    // Allows multiple calls to notifyModelDownloadComplete to work as expected
    // Also, we don't want to signal model download end if there was no start
    if (!this.#lastDownloadOk.has(sessionId)) {
      return;
    }
    const isSuccess = this.#lastDownloadOk.get(sessionId);
    const step = isSuccess ? "end_download_success" : "end_download_failed";
    this.#lastDownloadOk.delete(sessionId);
    Glean.firefoxAiRuntime.modelDownload.record({
      modelDownloadId: sessionId,
      featureId,
      engineId,
      modelId: model,
      step,
      duration: 0,
      modelRevision: revision,
      error: isSuccess
        ? ""
        : "Unable to retrieve all files needed for the model to work",
    });
  }

  /**
   * Given an organization, model, and version, fetch a model file in the hub
   * while supporting status callback.
   *
   * @param {object} config
   * @param {string} config.engineId - The model engine id
   * @param {string} config.taskName - name of the inference task.
   * @param {string} config.model - The model name (organization/name).
   * @param {string} config.revision - The model revision.
   * @param {string} config.file - The file name.
   * @param {string} config.modelHubRootUrl - root url of the model hub
   * @param {string} config.modelHubUrlTemplate - url template of the model hub
   * @param {?function(ProgressAndStatusCallbackParams):void} config.progressCallback A function to call to indicate progress status.
   * @param {string} config.featureId - feature id for the model
   * @param {string} config.sessionId - shared across the same session
   * @param {object} config.telemetryData - Additional telemetry data.
   * @returns {Promise<[string, headers]>} The local path to the file content and headers.
   */
  async getModelDataAsFile({
    engineId,
    taskName,
    model,
    revision,
    file,
    modelHubRootUrl,
    modelHubUrlTemplate,
    progressCallback,
    featureId,
    sessionId,
    telemetryData = {},
  }) {
    // Make sure inputs are clean. We don't sanitize them but throw an exception
    let checkError = ModelHub.checkInput(model, revision, file);
    if (checkError) {
      throw checkError;
    }
    const url = this.#fileUrl({
      model,
      revision,
      file,
      modelHubRootUrl,
      modelHubUrlTemplate,
    });
    lazy.console.debug(`Getting model file from ${url}`);

    await this.#initCache();

    // we store the hostname alongside the model so we can distinguished per hub
    const hostname = new URL(url).hostname;
    const modelWithHostname = `${hostname}/${model}`;

    let useCached;
    const chromeFile = url.startsWith("chrome://");
    const fileAllowed = this.allowDenyList?.allowedURL(url);
    let cachedHeaders = null;

    // If the revision is `main` we want to check the ETag in the hub
    if (revision === "main" && !chromeFile) {
      // this can be null if no ETag was found or there were a network error
      const hubETag = await this.getETag(url);

      // Storage ETag lookup
      cachedHeaders = await this.cache.getHeaders({
        model: modelWithHostname,
        revision,
        file,
      });
      const cachedEtag = cachedHeaders ? cachedHeaders.ETag : null;

      // If we have something in store, and the hub ETag is null or it matches the cached ETag, return the cached response
      useCached =
        cachedEtag !== null && (hubETag === null || cachedEtag === hubETag);
    } else {
      // If we are dealing with a pinned revision, we ignore the ETag, to spare HEAD hits on every call
      useCached = await this.cache.fileExists({
        model: modelWithHostname,
        revision,
        file,
      });
    }

    const progressInfo = {
      progress: null,
      totalLoaded: null,
      currentLoaded: null,
      total: null,
    };

    const statusInfo = {
      metadata: { model, revision, file, url, taskName },
      ok: true,
      id: url,
    };

    const localFilePath = this.cache.generateFilePathInOPFS({
      model: modelWithHostname,
      revision,
      file,
    });

    if (useCached) {
      if (!fileAllowed.allowed) {
        await this.cache.deleteModels({
          model,
          revision,
          deletedBy: "denylist",
        });
        throw new ForbiddenURLError(url, fileAllowed.rejectionType);
      }
      lazy.console.debug(`Cache Hit for ${url}`);
      progressCallback?.(
        new lazy.Progress.ProgressAndStatusCallbackParams({
          ...statusInfo,
          ...progressInfo,
          type: lazy.Progress.ProgressType.LOAD_FROM_CACHE,
          statusText: lazy.Progress.ProgressStatusText.INITIATE,
        })
      );

      if (!cachedHeaders) {
        cachedHeaders = await this.cache.getHeaders({
          model: modelWithHostname,
          revision,
          file,
        });
      }

      // Ensure that we indicate that the taskName is stored
      await this.cache.updateTask({
        engineId,
        taskName,
        model: modelWithHostname,
        revision,
        file,
      });

      progressCallback?.(
        new lazy.Progress.ProgressAndStatusCallbackParams({
          ...statusInfo,
          ...progressInfo,
          type: lazy.Progress.ProgressType.LOAD_FROM_CACHE,
          statusText: lazy.Progress.ProgressStatusText.DONE,
        })
      );

      cachedHeaders.lastUsed = Date.now();
      await this.cache.setHeaders({ model, revision, file, cachedHeaders });

      return [localFilePath, cachedHeaders];
    }

    progressCallback?.(
      new lazy.Progress.ProgressAndStatusCallbackParams({
        ...statusInfo,
        ...progressInfo,
        type: lazy.Progress.ProgressType.DOWNLOAD,
        statusText: lazy.Progress.ProgressStatusText.INITIATE,
      })
    );

    if (!this.#lastDownloadOk.has(sessionId)) {
      Glean.firefoxAiRuntime.modelDownload.record({
        modelDownloadId: sessionId,
        featureId,
        engineId,
        modelId: model,
        step: "start_download",
        duration: 0,
        modelRevision: revision,
        error: "",
        ...telemetryData,
      });
    }
    this.#lastDownloadOk.set(sessionId, false);

    const start = Date.now();
    Glean.firefoxAiRuntime.modelDownload.record({
      modelDownloadId: sessionId,
      featureId,
      engineId,
      modelId: model,
      step: "start_file_download",
      duration: 0,
      modelRevision: revision,
      error: "",
      ...telemetryData,
    });

    lazy.console.debug(`Fetching ${url}`);
    let caughtError;
    try {
      let isFirstCall = true;
      const response = await this.#fetch(url);
      const fileObject = await lazy.OPFS.download({
        savePath: localFilePath,
        deletePreviousVersions: false,
        skipIfExists: false,
        source: response,
        progressCallback: progressData => {
          progressCallback?.(
            new lazy.Progress.ProgressAndStatusCallbackParams({
              ...progressInfo,
              ...progressData,
              statusText: isFirstCall
                ? lazy.Progress.ProgressStatusText.SIZE_ESTIMATE
                : lazy.Progress.ProgressStatusText.IN_PROGRESS,
              type: lazy.Progress.ProgressType.DOWNLOAD,
              ...statusInfo,
            })
          );
          isFirstCall = false;
        },
      });

      this.#lastDownloadOk.set(sessionId, true);
      const end = Date.now();
      const duration = Math.floor(end - start);
      Glean.firefoxAiRuntime.modelDownload.record({
        modelDownloadId: sessionId,
        featureId,
        engineId,
        modelId: model,
        step: "end_file_download_success",
        duration,
        modelRevision: revision,
        error: "",
        ...telemetryData,
      });

      const headers = this.extractHeaders(response);
      headers.fileSize = fileObject.size;

      await this.cache.put({
        engineId,
        taskName,
        model: modelWithHostname,
        revision,
        file,
        data: localFilePath,
        headers,
      });

      progressCallback?.(
        new lazy.Progress.ProgressAndStatusCallbackParams({
          ...statusInfo,
          ...progressInfo,
          type: lazy.Progress.ProgressType.DOWNLOAD,
          statusText: lazy.Progress.ProgressStatusText.DONE,
        })
      );

      return [localFilePath, headers];
    } catch (error) {
      caughtError = error;
      const end = Date.now();
      const duration = Math.floor(end - start);
      Glean.firefoxAiRuntime.modelDownload.record({
        modelDownloadId: sessionId,
        featureId,
        engineId,
        modelId: model,
        step: "end_file_download_failed",
        duration,
        modelRevision: revision,
        error: error.constructor.name,
        ...telemetryData,
      });

      lazy.console.error(`Failed to fetch ${url}:`, error);
    }

    // Indicate there is an error
    progressCallback?.(
      new lazy.Progress.ProgressAndStatusCallbackParams({
        ...statusInfo,
        ...progressInfo,
        type: lazy.Progress.ProgressType.DOWNLOAD,
        statusText: lazy.Progress.ProgressStatusText.DONE,
        ok: false,
      })
    );

    throw new Error(
      `Failed to fetch the model file: ${url}. Reason: ${caughtError.message} ${caughtError.stack}`,
      {
        cause: caughtError,
      }
    );
  }

  /**
   * Lists all models stored in the hub.
   *
   * @returns {Promise<Array<{name: string, revision: string}>>}
   */
  async listModels() {
    if (lazy.isPrivateBrowsing()) {
      lazy.console.debug(
        "Returning an empty list of models for private windows"
      );
      return [];
    }
    await this.#initCache();
    return this.cache.listModels();
  }

  /**
   * Lists all files for a given model and revision stored in the cache.
   *
   * @param {object} config
   * @param {?string} config.model - The model name (hostname/organization/name).
   * @param {?string} config.revision - The model version.
   * @param {?string} config.taskName - name of the inference :wtask.
   * @returns {Promise<Array<{path:string, headers: object}>>} An array of file identifiers.
   */
  async listFiles({ taskName, model, revision }) {
    await this.#initCache();
    return this.cache.listFiles({ taskName, model, revision });
  }

  /**
   * Deletes all data related to the specifed models.
   *
   * @param {object} config
   *
   * @param {?string} config.model - The model name (hostname/organization/name) to delete.
   * @param {?string} config.revision - The model version to delete.
   *  If both model and revision are null, delete models of any name and version.
   *
   * @param {?string} config.taskName - name of the inference task to delete.
   *                                    If null, delete specified models for all tasks.
   *
   * @param {?function(IDBCursor):boolean} config.filterFn - A function to execute for each model file candidate for deletion.
   * @param {?string} config.deletedBy - The feature who deleted the model
   * It should return a truthy value to delete the model file, and a falsy value otherwise.
   *
   * @throws {Error} If a revision is defined, the model must also be defined.
   *                 If the model is not defined, the revision should also not be defined.
   *                 Otherwise, an error will be thrown.

   * @returns {Promise<void>}
   */
  async deleteModels({
    taskName,
    model,
    revision,
    filterFn,
    deletedBy = "other",
  }) {
    await this.#initCache();
    return this.cache.deleteModels({
      taskName,
      model,
      revision,
      filterFn,
      deletedBy,
    });
  }

  /**
   * Deletes files associated with a specific engine ID in the cache.
   *
   * @param {object} config
   *
   * @param {?string} config.engineId - The ID of the engine whose files are to be deleted.
   * @param {?string} config.deletedBy - The feature who deleted the model
   *
   * @returns {Promise<void>} A promise that resolves once the deletion process is complete.
   */
  async deleteFilesByEngine({ engineId, deletedBy = "other" }) {
    await this.#initCache();
    return this.cache.deleteFilesByEngine({ engineId, deletedBy });
  }

  /**
   * Returns the owner icon from a model
   *
   * @param {string} model -- Fully qualified model name
   * @returns {Promise<string|null>}
   */
  async getOwnerIcon(model) {
    await this.#initCache();
    const owner = ModelOwner.fromModel(model);
    return owner.getIcon();
  }
}
