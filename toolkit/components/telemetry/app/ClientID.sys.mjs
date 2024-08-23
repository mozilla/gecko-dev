/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Log } from "resource://gre/modules/Log.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "ClientID::";
// Must match ID in TelemetryUtils
const CANARY_CLIENT_ID = "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0";
const CANARY_PROFILE_GROUP_ID = "decafdec-afde-cafd-ecaf-decafdecafde";

const DRS_STATE_VERSION = 2;

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CommonUtils: "resource://services-common/utils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "CryptoHash", () => {
  return Components.Constructor(
    "@mozilla.org/security/hash;1",
    "nsICryptoHash",
    "initWithString"
  );
});

ChromeUtils.defineLazyGetter(lazy, "gDatareportingPath", () => {
  return PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "datareporting"
  );
});

ChromeUtils.defineLazyGetter(lazy, "gStateFilePath", () => {
  return PathUtils.join(lazy.gDatareportingPath, "state.json");
});

const PREF_CACHED_CLIENTID = "toolkit.telemetry.cachedClientID";
const PREF_CACHED_PROFILEGROUPID = "toolkit.telemetry.cachedProfileGroupID";

/**
 * Checks if the string is a valid UUID (without braces).
 *
 * @param {String} id A string containing an ID.
 * @return {Boolean} True when the ID has valid format, or False otherwise.
 */
function isValidUUID(id) {
  const UUID_REGEX =
    /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
  return UUID_REGEX.test(id);
}

export var ClientID = Object.freeze({
  /**
   * This returns a promise resolving to the stable client ID we use for
   * data reporting.
   *
   * @return {Promise<string>} The stable client ID.
   */
  getClientID() {
    return ClientIDImpl.getClientID();
  },

  /**
   * This returns a promise resolving to the stable profile group ID we use for
   * data reporting.
   *
   * @return {Promise<string>} The stable profile group ID.
   */
  getProfileGroupID() {
    return ClientIDImpl.getProfileGroupID();
  },

  /**
   * Asynchronously updates the stable profile group ID we use for data
   * reporting.
   *
   * @param {String} id A string containing the profile group ID.
   * @return {Promise} Resolves when the ID is updated.
   */
  setProfileGroupID(id) {
    return ClientIDImpl.setProfileGroupID(id);
  },

  /**
   * Get the client id synchronously without hitting the disk.
   * This returns:
   *  - the current on-disk client id if it was already loaded
   *  - the client id that we cached into preferences (if any)
   *  - null otherwise
   */
  getCachedClientID() {
    return ClientIDImpl.getCachedClientID();
  },

  /**
   * Get the profile group ID synchronously without hitting the disk.
   * This returns:
   *  - the current on-disk profile group ID if it was already loaded
   *  - the profile group ID that we cached into preferences (if any)
   *  - null otherwise
   */
  getCachedProfileGroupID() {
    return ClientIDImpl.getCachedProfileGroupID();
  },

  async getClientIdHash() {
    return ClientIDImpl.getClientIdHash();
  },

  /**
   * Sets the client ID and profile group ID to the canary (known) identifiers,
   * writing it to disk and updating the cached version.
   *
   * Use `resetIdentifiers` to clear the existing identifiers and generate new,
   * random ones if required.
   *
   * @return {Promise<void>}
   */
  setCanaryIdentifiers() {
    return ClientIDImpl.setCanaryIdentifiers();
  },

  /**
   * Assigns new random values to client ID and profile group ID. Should only be
   * used if a reset is explicitly requested by the user.
   *
   * @return {Promise<void>}
   */
  resetIdentifiers() {
    return ClientIDImpl.resetIdentifiers();
  },

  /**
   * Only used for testing. Invalidates the cached client ID and profile group
   * ID so that they are read again from file, but doesn't remove the existing
   * IDs from disk.
   */
  _reset() {
    return ClientIDImpl._reset();
  },
});

var ClientIDImpl = {
  _clientID: null,
  _clientIDHash: null,
  _profileGroupID: null,
  _loadClientIdTask: null,
  _saveDataReportingStateTask: null,
  _resetIdentifiersTask: null,
  _logger: null,

  _loadDataReportingState() {
    if (this._loadClientIdTask) {
      return this._loadClientIdTask;
    }

    this._loadClientIdTask = this._doLoadDataReportingState();
    let clear = () => (this._loadClientIdTask = null);
    this._loadClientIdTask.then(clear, clear);
    return this._loadClientIdTask;
  },

  /**
   * Load the client ID and profile group ID from the DataReporting Service
   * state file. If either is missing, we generate a new one.
   */
  async _doLoadDataReportingState() {
    this._log.trace(`_doLoadDataReportingState`);
    // If there's a removal in progress, let's wait for it
    await this._resetIdentifiersTask;

    // Try to load the client id from the DRS state file.
    let hasCurrentClientID = false;
    let hasCurrentProfileGroupID = false;
    try {
      let state = await IOUtils.readJSON(lazy.gStateFilePath);
      if (state) {
        if (!("version" in state)) {
          // Old version, clear out any previously generated profile group ID.
          delete state.profileGroupID;
        }

        hasCurrentClientID = this.updateClientID(state.clientID);
        hasCurrentProfileGroupID = this.updateProfileGroupID(
          state.profileGroupID
        );

        if (!hasCurrentProfileGroupID && hasCurrentClientID) {
          // A pre-existing profile should be assigned the existing client ID.
          hasCurrentProfileGroupID = this.updateProfileGroupID(this._clientID);

          if (hasCurrentProfileGroupID) {
            this._saveDataReportingStateTask = this._saveDataReportingState();
            await this._saveDataReportingStateTask;
          }
        }

        if (hasCurrentClientID && hasCurrentProfileGroupID) {
          this._log.trace(
            `_doLoadDataReportingState: Client and Group IDs loaded from state.`
          );
          return {
            clientID: this._clientID,
            profileGroupID: this._profileGroupID,
          };
        }
      }
    } catch (e) {
      // fall through to next option
    }

    // Absent or broken state file? Check prefs as last resort.
    if (!hasCurrentClientID) {
      const cachedID = this.getCachedClientID();
      // Calling `updateClientID` with `null` logs an error, which breaks tests.
      if (cachedID) {
        hasCurrentClientID = this.updateClientID(cachedID);
      }
    }

    if (!hasCurrentProfileGroupID) {
      const cachedID = this.getCachedProfileGroupID();
      // Calling `updateProfileGroupID` with `null` logs an error, which breaks tests.
      if (cachedID) {
        hasCurrentProfileGroupID = this.updateProfileGroupID(cachedID);
      }
    }

    // We're missing the ID from the DRS state file and prefs.
    // Generate a new one.
    if (!hasCurrentClientID) {
      this.updateClientID(lazy.CommonUtils.generateUUID());
    }

    if (!hasCurrentProfileGroupID) {
      this.updateProfileGroupID(lazy.CommonUtils.generateUUID());
    }

    this._saveDataReportingStateTask = this._saveDataReportingState();

    // Wait on persisting the id. Otherwise failure to save the ID would result in
    // the client creating and subsequently sending multiple IDs to the server.
    // This would appear as multiple clients submitting similar data, which would
    // result in orphaning.
    await this._saveDataReportingStateTask;

    this._log.trace(
      "_doLoadDataReportingState: client and profile group IDs loaded and persisted."
    );
    return {
      clientID: this._clientID,
      profileGroupID: this._profileGroupID,
    };
  },

  /**
   * Save the client and profile group IDs to the DRS state file.
   *
   * @return {Promise} A promise resolved when the DRS state file is saved to disk.
   */
  async _saveDataReportingState() {
    try {
      this._log.trace(`_saveDataReportingState`);
      let obj = {
        version: DRS_STATE_VERSION,
        clientID: this._clientID,
        profileGroupID: this._profileGroupID,
      };
      await IOUtils.makeDirectory(lazy.gDatareportingPath);
      await IOUtils.writeJSON(lazy.gStateFilePath, obj, {
        tmpPath: `${lazy.gStateFilePath}.tmp`,
      });
      this._saveDataReportingStateTask = null;
    } catch (ex) {
      if (!DOMException.isInstance(ex) || ex.name !== "AbortError") {
        throw ex;
      }
    }
  },

  /**
   * This returns a promise resolving to the stable client ID we use for
   * data reporting (FHR & Telemetry).
   *
   * @return {Promise<string>} The stable client ID.
   */
  async getClientID() {
    if (!this._clientID) {
      let { clientID } = await this._loadDataReportingState();
      if (AppConstants.platform != "android") {
        Glean.legacyTelemetry.clientId.set(clientID);
      }
      return clientID;
    }

    return Promise.resolve(this._clientID);
  },

  /**
   * This returns a promise resolving to the stable profile group ID we use for
   * data reporting (FHR & Telemetry).
   *
   * @return {Promise<string>} The stable profile group ID.
   */
  async getProfileGroupID() {
    if (!this._profileGroupID) {
      let { profileGroupID } = await this._loadDataReportingState();
      if (AppConstants.platform != "android") {
        Glean.legacyTelemetry.profileGroupId.set(profileGroupID);
      }
      return profileGroupID;
    }

    return this._profileGroupID;
  },

  /**
   * Asynchronously updates the stable profile group ID we use for data reporting
   * (FHR & Telemetry).
   *
   * @param {String} id A string containing the profile group ID.
   * @return {Promise} Resolves when the ID is updated.
   */
  async setProfileGroupID(id) {
    // Make sure that we have loaded the client ID. Do this before updating the
    // profile group ID as loading would clobber that.
    if (!this._clientID) {
      await this._loadDataReportingState();
    }

    if (!(await this.updateProfileGroupID(id))) {
      this._log.error(
        "setProfileGroupID - invalid profile group ID passed, not updating"
      );
      throw new Error("Invalid profile group ID");
    }

    // If there is already a save in progress, wait for it to complete.
    await this._saveDataReportingStateTask;

    this._saveDataReportingStateTask = this._saveDataReportingState();
    await this._saveDataReportingStateTask;
  },

  /**
   * Get the client id synchronously without hitting the disk.
   * This returns:
   *  - the current on-disk client id if it was already loaded
   *  - the client id that we cached into preferences (if any)
   *  - null otherwise
   */
  getCachedClientID() {
    if (this._clientID) {
      // Already loaded the client id from disk.
      return this._clientID;
    }

    // If the client id cache contains a value of the wrong type,
    // reset the pref. We need to do this before |getStringPref| since
    // it will just return |null| in that case and we won't be able
    // to distinguish between the missing pref and wrong type cases.
    if (
      Services.prefs.prefHasUserValue(PREF_CACHED_CLIENTID) &&
      Services.prefs.getPrefType(PREF_CACHED_CLIENTID) !=
        Ci.nsIPrefBranch.PREF_STRING
    ) {
      this._log.error(
        "getCachedClientID - invalid client id type in preferences, resetting"
      );
      Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
    }

    // Not yet loaded, return the cached client id if we have one.
    let id = Services.prefs.getStringPref(PREF_CACHED_CLIENTID, null);
    if (id === null) {
      return null;
    }
    if (!isValidUUID(id)) {
      this._log.error(
        "getCachedClientID - invalid client id in preferences, resetting",
        id
      );
      Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
      return null;
    }
    return id;
  },

  /**
   * Get the profile group ID synchronously without hitting the disk.
   * This returns:
   *  - the current on-disk profile group ID if it was already loaded
   *  - the profile group ID that we cached into preferences (if any)
   *  - null otherwise
   */
  getCachedProfileGroupID() {
    if (this._profileGroupID) {
      // Already loaded the profile group ID from disk.
      return this._profileGroupID;
    }

    // If the profile group ID cache contains a value of the wrong type,
    // reset the pref. We need to do this before |getStringPref| since
    // it will just return |null| in that case and we won't be able
    // to distinguish between the missing pref and wrong type cases.
    if (
      Services.prefs.prefHasUserValue(PREF_CACHED_PROFILEGROUPID) &&
      Services.prefs.getPrefType(PREF_CACHED_PROFILEGROUPID) !=
        Ci.nsIPrefBranch.PREF_STRING
    ) {
      this._log.error(
        "getCachedProfileGroupID - invalid profile group ID type in preferences, resetting"
      );
      Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
    }

    // Not yet loaded, return the cached profile group ID if we have one.
    let id = Services.prefs.getStringPref(PREF_CACHED_PROFILEGROUPID, null);
    if (id === null) {
      return null;
    }
    if (!isValidUUID(id)) {
      this._log.error(
        "getCachedProfileGroupID - invalid profile group ID in preferences, resetting",
        id
      );
      Services.prefs.clearUserPref(PREF_CACHED_PROFILEGROUPID);
      return null;
    }
    return id;
  },

  async getClientIdHash() {
    if (!this._clientIDHash) {
      let byteArr = new TextEncoder().encode(await this.getClientID());
      let hash = new lazy.CryptoHash("sha256");
      hash.update(byteArr, byteArr.length);
      this._clientIDHash = lazy.CommonUtils.bytesAsHex(hash.finish(false));
    }
    return this._clientIDHash;
  },

  /**
   * Resets the module. This is for testing only.
   */
  async _reset() {
    await this._loadClientIdTask;
    await this._saveDataReportingStateTask;
    this._clientID = null;
    this._clientIDHash = null;
    this._profileGroupID = null;
  },

  async setCanaryIdentifiers() {
    this._log.trace("setCanaryIdentifiers");
    this.updateClientID(CANARY_CLIENT_ID);
    this.updateProfileGroupID(CANARY_PROFILE_GROUP_ID);

    this._saveDataReportingStateTask = this._saveDataReportingState();
    await this._saveDataReportingStateTask;
    return this._clientID;
  },

  async _doResetIdentifiers() {
    this._log.trace("_doResetIdentifiers");

    // Reset the cached client ID.
    this.updateClientID(lazy.CommonUtils.generateUUID());
    this._clientIDHash = null;

    // Reset the cached profile group ID.
    this.updateProfileGroupID(lazy.CommonUtils.generateUUID());

    // If there is a save in progress, wait for it to complete.
    await this._saveDataReportingStateTask;

    // Save the new identifiers to disk.
    this._saveDataReportingStateTask = this._saveDataReportingState();
    await this._saveDataReportingStateTask;
  },

  async resetIdentifiers() {
    this._log.trace("resetIdentifiers");

    // Wait for the removal.
    // Asynchronous calls to getClientID will also be blocked on this.
    this._resetIdentifiersTask = this._doResetIdentifiers();
    let clear = () => (this._resetIdentifiersTask = null);
    this._resetIdentifiersTask.then(clear, clear);

    await this._resetIdentifiersTask;
  },

  /**
   * Sets the client id to the given value and updates the value cached in
   * preferences only if the given id is a valid UUID.
   *
   * @param {String} id A string containing the client ID.
   * @return {Boolean} True when the client ID has valid format, or False
   * otherwise.
   */
  updateClientID(id) {
    if (!isValidUUID(id)) {
      this._log.error("updateClientID - invalid client ID", id);
      return false;
    }

    this._clientID = id;
    if (AppConstants.platform != "android") {
      Glean.legacyTelemetry.clientId.set(id);
    }

    this._clientIDHash = null;
    Services.prefs.setStringPref(PREF_CACHED_CLIENTID, this._clientID);
    return true;
  },

  /**
   * Sets the profile group ID to the given value and updates the value cached
   * in preferences only if the given id is a valid UUID.
   *
   * @param {String} id A string containing the profile group ID.
   * @return {Boolean} True when the profile group ID has valid format, or False
   * otherwise.
   */
  updateProfileGroupID(id) {
    if (!isValidUUID(id)) {
      this._log.error("updateProfileGroupID - invalid profile group ID", id);
      return false;
    }

    this._profileGroupID = id;
    if (AppConstants.platform != "android") {
      Glean.legacyTelemetry.profileGroupId.set(id);
    }

    Services.prefs.setStringPref(
      PREF_CACHED_PROFILEGROUPID,
      this._profileGroupID
    );
    return true;
  },

  /**
   * A helper for getting access to telemetry logger.
   */
  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(
        LOGGER_NAME,
        LOGGER_PREFIX
      );
    }

    return this._logger;
  },
};
