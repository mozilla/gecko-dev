/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  IndexedDB: "resource://gre/modules/IndexedDB.sys.mjs",
  ProfilesDatastoreService:
    "moz-src:///toolkit/profile/ProfilesDatastoreService.sys.mjs",
  ASRouterPreferences:
    "resource:///modules/asrouter/ASRouterPreferences.sys.mjs",
});

export class ASRouterStorage {
  /**
   * @param storeNames Array of strings used to create all the required stores
   */
  constructor({ storeNames, telemetry }) {
    if (!storeNames) {
      throw new Error("storeNames required");
    }

    this.dbName = "ActivityStream";
    this.dbVersion = 3;
    this.storeNames = storeNames;
    this.telemetry = telemetry;
  }

  get db() {
    return this._db || (this._db = this.createOrOpenDb());
  }

  /**
   * Public method that binds the store required by the consumer and exposes
   * the private db getters and setters.
   *
   * @param storeName String name of desired store
   */
  getDbTable(storeName) {
    if (this.storeNames.includes(storeName)) {
      return {
        get: this._get.bind(this, storeName),
        getAll: this._getAll.bind(this, storeName),
        getAllKeys: this._getAllKeys.bind(this, storeName),
        set: this._set.bind(this, storeName),
        getSharedMessageImpressions:
          this.getSharedMessageImpressions.bind(this),
        getSharedMessageBlocklist: this.getSharedMessageBlocklist.bind(this),
        setSharedMessageImpressions:
          this.setSharedMessageImpressions.bind(this),
        setSharedMessageBlocked: this.setSharedMessageBlocked.bind(this),
      };
    }

    throw new Error(`Store name ${storeName} does not exist.`);
  }

  async _getStore(storeName) {
    return (await this.db).objectStore(storeName, "readwrite");
  }

  _get(storeName, key) {
    return this._requestWrapper(async () =>
      (await this._getStore(storeName)).get(key)
    );
  }

  _getAll(storeName) {
    return this._requestWrapper(async () =>
      (await this._getStore(storeName)).getAll()
    );
  }

  _getAllKeys(storeName) {
    return this._requestWrapper(async () =>
      (await this._getStore(storeName)).getAllKeys()
    );
  }

  _set(storeName, key, value) {
    return this._requestWrapper(async () =>
      (await this._getStore(storeName)).put(value, key)
    );
  }

  _openDatabase() {
    return lazy.IndexedDB.open(this.dbName, this.dbVersion, db => {
      // If provided with array of objectStore names we need to create all the
      // individual stores
      this.storeNames.forEach(store => {
        if (!db.objectStoreNames.contains(store)) {
          this._requestWrapper(() => db.createObjectStore(store));
        }
      });
    });
  }

  /**
   * createOrOpenDb - Open a db (with this.dbName) if it exists.
   *                  If it does not exist, create it.
   *                  If an error occurs, deleted the db and attempt to
   *                  re-create it.
   * @returns Promise that resolves with a db instance
   */
  async createOrOpenDb() {
    try {
      const db = await this._openDatabase();
      return db;
    } catch (e) {
      if (this.telemetry) {
        this.telemetry.handleUndesiredEvent({ event: "INDEXEDDB_OPEN_FAILED" });
      }
      await lazy.IndexedDB.deleteDatabase(this.dbName);
      return this._openDatabase();
    }
  }

  async _requestWrapper(request) {
    let result = null;
    try {
      result = await request();
    } catch (e) {
      if (this.telemetry) {
        this.telemetry.handleUndesiredEvent({ event: "TRANSACTION_FAILED" });
      }
      throw e;
    }

    return result;
  }

  /**
   * Gets all of the message impression data
   * @returns {object|null} All multiprofile message impressions or null if error occurs
   */
  async getSharedMessageImpressions() {
    const conn = await lazy.ProfilesDatastoreService.getConnection();
    if (!conn) {
      return null;
    }
    try {
      const rows = await conn.executeCached(
        `SELECT messageId, json(impressions) AS impressions FROM MessagingSystemMessageImpressions;`
      );

      if (rows.length === 0) {
        return null;
      }

      const impressionsData = {};

      for (const row of rows) {
        const messageId = row.getResultByName("messageId");
        const impressions = JSON.parse(row.getResultByName("impressions"));

        impressionsData[messageId] = impressions;
      }

      return impressionsData;
    } catch (e) {
      lazy.ASRouterPreferences.console.error(
        `ASRouterStorage: Failed reading from MessagingSystemMessageImpressions`,
        e
      );
      if (this.telemetry) {
        this.telemetry.handleUndesiredEvent({
          event: "SHARED_DB_READ_FAILED",
        });
      }
      return null;
    }
  }

  /**
   * Gets the message blocklist
   * @returns {Array|null} The message blocklist, or null if error occurred
   */
  async getSharedMessageBlocklist() {
    const conn = await lazy.ProfilesDatastoreService.getConnection();
    if (!conn) {
      return null;
    }
    try {
      const rows = await conn.executeCached(
        `SELECT messageId FROM MessagingSystemMessageBlocklist;`
      );

      return rows.map(row => row.getResultByName("messageId"));
    } catch (e) {
      lazy.ASRouterPreferences.console.error(
        `ASRouterStorage: Failed reading from MessagingSystemMessageBlocklist`,
        e
      );
      if (this.telemetry) {
        this.telemetry.handleUndesiredEvent({
          event: "SHARED_DB_READ_FAILED",
        });
      }
      return null;
    }
  }

  /**
   * Set the message impressions for a given message ID
   * @param {string} messageId - The message ID to set the impressions for
   * @param {Array|null} impressions - The new value of "impressions" (an array of
   *  impression data or an emtpy array, or null to delete)
   * @returns {boolean} Success status
   */
  async setSharedMessageImpressions(messageId, impressions) {
    let success = true;
    const conn = await lazy.ProfilesDatastoreService.getConnection();
    if (!conn) {
      return false;
    }
    try {
      if (!messageId) {
        throw new Error(
          "Failed attempt to set shared message impressions with no message ID."
        );
      }

      // If impressions is falsy or an empty array, delete the row
      if (
        !impressions ||
        (Array.isArray(impressions) && impressions.length === 0)
      ) {
        await conn.executeBeforeShutdown(
          "ASRouter: setSharedMessageImpressions",
          async () => {
            await conn.executeCached(
              `DELETE FROM MessagingSystemMessageImpressions WHERE messageId = :messageId;`,
              {
                messageId,
              }
            );
          }
        );
      } else {
        await conn.executeBeforeShutdown(
          "ASRouter: setSharedMessageImpressions",
          async () => {
            await conn.executeCached(
              `INSERT INTO MessagingSystemMessageImpressions (messageId, impressions) VALUES (
                :messageId,
                jsonb(:impressions)
              )
              ON CONFLICT (messageId) DO UPDATE SET impressions = excluded.impressions;`,
              {
                messageId,
                impressions: JSON.stringify(impressions),
              }
            );
          }
        );
      }

      lazy.ProfilesDatastoreService.notify();
    } catch (e) {
      lazy.ASRouterPreferences.console.error(
        `ASRouterStorage: Failed writing to MessagingSystemMessageImpressions`,
        e
      );
      if (this.telemetry) {
        this.telemetry.handleUndesiredEvent({
          event: "SHARED_DB_WRITE_FAILED",
        });
      }
      success = false;
    }

    return success;
  }

  /**
   * Adds a message ID to the blocklist and removes impressions
   * for that message ID from the impressions table when isBlocked is true
   * and deletes message ID from the blocklist when isBlocked is false
   * @param {string} messageId - The message ID to set the blocked status for
   * @param {boolean} [isBlocked=true] - If the message should be blocked (true) or unblocked (false)
   * @returns {boolean} Success status
   */
  async setSharedMessageBlocked(messageId, isBlocked = true) {
    let success = true;
    const conn = await lazy.ProfilesDatastoreService.getConnection();
    if (!conn) {
      return false;
    }
    if (isBlocked) {
      // Block the message, and clear impressions
      try {
        await conn.executeTransaction(async () => {
          await conn.executeCached(
            `INSERT INTO MessagingSystemMessageBlocklist (messageId)
                VALUES (:messageId);`,
            {
              messageId,
            }
          );
          await conn.executeCached(
            `DELETE FROM MessagingSystemMessageImpressions
                WHERE messageId = :messageId;`,
            {
              messageId,
            }
          );
        });
      } catch (e) {
        lazy.ASRouterPreferences.console.error(
          `ASRouterStorage: Failed writing to MessagingSystemMessageBlocklist`,
          e
        );
        if (this.telemetry) {
          this.telemetry.handleUndesiredEvent({
            event: "SHARED_DB_WRITE_FAILED",
          });
        }
        success = false;
      }
    } else {
      // Unblock the message
      try {
        await conn.executeBeforeShutdown(
          "ASRouter: setSharedMessageBlocked",
          async () => {
            await conn.executeCached(
              `DELETE FROM MessagingSystemMessageBlocklist WHERE messageId = :messageId;`,
              {
                messageId,
              }
            );
          }
        );
      } catch (e) {
        lazy.ASRouterPreferences.console.error(
          `ASRouterStorage: Failed writing to MessagingSystemMessageBlocklist`,
          e
        );
        if (this.telemetry) {
          this.telemetry.handleUndesiredEvent({
            event: "SHARED_DB_WRITE_FAILED",
          });
        }
        success = false;
      }
    }

    lazy.ProfilesDatastoreService.notify();
    return success;
  }
}

export function getDefaultOptions(options) {
  return { collapsed: !!options.collapsed };
}
