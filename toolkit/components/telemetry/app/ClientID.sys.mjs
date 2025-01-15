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
const CANARY_USAGE_PROFILE_ID = "beefbeef-beef-beef-beef-beeefbeefbee";

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
const PREF_CACHED_USAGE_PROFILEID = "datareporting.dau.cachedUsageProfileID";

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
   * This returns a promise resolving to the stable Usage Profile ID we use for
   * DAU reporting.
   *
   * @return {Promise<string>} The stable Usage Profile ID.
   */
  getUsageProfileID() {
    return ClientIDImpl.getUsageProfileID();
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
   * Asynchronously updates the stable Usage Profile ID we use for DAU
   * reporting.
   *
   * @param {String} id A string containing the Usage Profile ID.
   * @return {Promise} Resolves when the ID is updated.
   */
  setUsageProfileID(id) {
    return ClientIDImpl.setUsageProfileID(id);
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

  /**
   * Get the Usage Profile ID synchronously without hitting the disk.
   * This returns:
   *  - the current on-disk Usage Profile ID if it was already loaded
   *  - the Usage Profile ID that we cached into preferences (if any)
   *  - null otherwise
   */
  getCachedUsageProfileID() {
    return ClientIDImpl.getCachedUsageProfileID();
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
   * Sets the Usage Profile ID to the canary (known) identifier,
   * writing it to disk and updating the cached version.
   *
   * Does not touch the client ID or profile group ID.
   *
   * Use `resetUsageProfileIdentifier` to clear the existing identifier
   * and generate a new, random one if required.
   *
   * @return {Promise<void>}
   */
  setCanaryUsageProfileIdentifier() {
    return ClientIDImpl.setCanaryUsageProfileIdentifier();
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
   * Assigns a new random value to the Usage Profile ID.
   * Should only be used if a reset is explicitly requested by the user.
   *
   * Does not touch the client ID or profile group ID.
   *
   * @return {Promise<void>}
   */
  resetUsageProfileIdentifier() {
    return ClientIDImpl.resetUsageProfileIdentifier();
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
  _usageProfileID: null,
  _loadClientIdTask: null,
  _saveDataReportingStateTask: null,
  _resetIdentifiersTask: null,
  _resetUsageProfileIdentifierTask: null,
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
   * Load the client ID, profile group ID and Usage Profile ID from the DataReporting Service
   * state file. If any are missing, we generate a new one.
   */
  async _doLoadDataReportingState() {
    this._log.trace(`_doLoadDataReportingState`);
    // If there's a removal in progress, let's wait for it
    await this._resetIdentifiersTask;
    await this._resetUsageProfileIdentifierTask;

    // Try to load the client id from the DRS state file.
    let hasCurrentClientID = false;
    let hasCurrentProfileGroupID = false;
    let hasCurrentUsageProfileID = false;
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
        hasCurrentUsageProfileID = this.updateUsageProfileID(
          state.usageProfileID
        );

        if (!hasCurrentProfileGroupID && hasCurrentClientID) {
          // A pre-existing profile should be assigned the existing client ID.
          hasCurrentProfileGroupID = this.updateProfileGroupID(this._clientID);

          if (hasCurrentProfileGroupID) {
            this._saveDataReportingStateTask = this._saveDataReportingState();
            await this._saveDataReportingStateTask;
          }
        }

        if (
          hasCurrentClientID &&
          hasCurrentProfileGroupID &&
          hasCurrentUsageProfileID
        ) {
          this._log.trace(
            `_doLoadDataReportingState: Client ID, Profile Group ID and Usage Profile ID loaded from state.`
          );
          return {
            clientID: this._clientID,
            profileGroupID: this._profileGroupID,
            usageProfileID: this._usageProfileID,
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

    if (!hasCurrentUsageProfileID) {
      const cachedID = this.getCachedUsageProfileID();
      // Calling `updateUsageProfileID` with `null` logs an error, which breaks tests.
      if (cachedID) {
        hasCurrentUsageProfileID = this.updateUsageProfileID(cachedID);
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

    if (!hasCurrentUsageProfileID) {
      this.updateUsageProfileID(lazy.CommonUtils.generateUUID());
    }

    this._saveDataReportingStateTask = this._saveDataReportingState();

    // Wait on persisting the id. Otherwise failure to save the ID would result in
    // the client creating and subsequently sending multiple IDs to the server.
    // This would appear as multiple clients submitting similar data, which would
    // result in orphaning.
    await this._saveDataReportingStateTask;

    this._log.trace(
      "_doLoadDataReportingState: client ID, profile group ID and Usage Profile ID loaded and persisted."
    );
    return {
      clientID: this._clientID,
      profileGroupID: this._profileGroupID,
      usageProfileID: this._usageProfileID,
    };
  },

  /**
   * Save the client ID, profile group ID and Usage Profile ID to the DRS state file.
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
        usageProfileID: this._usageProfileID,
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
   * This returns a promise resolving to the stable Usage Profile ID we use for
   * DAU reporting.
   *
   * @return {Promise<string>} The stable Usage Profile ID.
   */
  async getUsageProfileID() {
    if (!this._usageProfileID) {
      let { usageProfileID } = await this._loadDataReportingState();
      if (AppConstants.platform != "android") {
        Glean.usage.profileId.set(usageProfileID);
      }
      return usageProfileID;
    }

    return this._usageProfileID;
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
   * Asynchronously updates the stable Usage Profile ID we use for DAU reporting.
   *
   * @param {String} id A string containing the Usage Profile ID.
   * @return {Promise} Resolves when the ID is updated.
   */
  async setUsageProfileID(id) {
    // Make sure that we have loaded the client ID. Do this before updating the
    // Usage Profile ID as loading would clobber that.
    if (!this._clientID) {
      await this._loadDataReportingState();
    }

    GleanPings.usageReporting.setEnabled(true);
    if (!(await this.updateUsageProfileID(id))) {
      this._log.error(
        "setUsageProfileID - invalid Usage Profile ID passed, not updating"
      );
      throw new Error("Invalid Usage Profile ID");
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

  /**
   * Get the Usage Profile ID synchronously without hitting the disk.
   * This returns:
   *  - the current on-disk Usage Profile ID if it was already loaded
   *  - the Usage Profile ID that we cached into preferences (if any)
   *  - null otherwise
   */
  getCachedUsageProfileID() {
    if (this._usageProfileID) {
      // Already loaded the Usage Profile ID from disk.
      return this._usageProfileID;
    }

    // If the Usage Profile ID cache contains a value of the wrong type,
    // reset the pref. We need to do this before |getStringPref| since
    // it will just return |null| in that case and we won't be able
    // to distinguish between the missing pref and wrong type cases.
    if (
      Services.prefs.prefHasUserValue(PREF_CACHED_USAGE_PROFILEID) &&
      Services.prefs.getPrefType(PREF_CACHED_USAGE_PROFILEID) !=
        Ci.nsIPrefBranch.PREF_STRING
    ) {
      this._log.error(
        "getCachedUsageProfileID - invalid Usage Profile ID type in preferences, resetting"
      );
      Services.prefs.clearUserPref(PREF_CACHED_USAGE_PROFILEID);
    }

    // Not yet loaded, return the cached Usage Profile ID if we have one.
    let id = Services.prefs.getStringPref(PREF_CACHED_USAGE_PROFILEID, null);
    if (id === null) {
      return null;
    }
    if (!isValidUUID(id)) {
      this._log.error(
        "getCachedUsageProfileID - invalid Usage Profile ID in preferences, resetting",
        id
      );
      Services.prefs.clearUserPref(PREF_CACHED_USAGE_PROFILEID);
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
    this._usageProfileID = null;
  },

  async setCanaryIdentifiers() {
    this._log.trace("setCanaryIdentifiers");
    this.updateClientID(CANARY_CLIENT_ID);
    this.updateProfileGroupID(CANARY_PROFILE_GROUP_ID);

    this._saveDataReportingStateTask = this._saveDataReportingState();
    await this._saveDataReportingStateTask;
    return this._clientID;
  },

  async setCanaryUsageProfileIdentifier() {
    this._log.trace("setCanaryUsageProfileIdentifier");
    this.updateUsageProfileID(CANARY_USAGE_PROFILE_ID);
    GleanPings.usageReporting.setEnabled(false);

    this._saveDataReportingStateTask = this._saveDataReportingState();
    await this._saveDataReportingStateTask;
    return this._usageProfileID;
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

  async _doResetUsageProfileIdentifier() {
    this._log.trace("_doResetUsageProfileIdentifier");

    // Reset the cached Usage Profile ID.
    GleanPings.usageReporting.setEnabled(true);
    this.updateUsageProfileID(lazy.CommonUtils.generateUUID());

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

  async resetUsageProfileIdentifier() {
    this._log.trace("resetUsageProfileIdentifier");

    // Wait for the removal.
    // Asynchronous calls to getClientID will also be blocked on this.
    this._resetUsageProfileIdentifierTask =
      this._doResetUsageProfileIdentifier();
    let clear = () => (this._resetUsageProfileIdentifierTask = null);
    this._resetUsageProfileIdentifierTask.then(clear, clear);

    await this._resetUsageProfileIdentifierTask;
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

  updateUsageProfileID(id) {
    if (!isValidUUID(id)) {
      this._log.error("updateUsageProfileID - invalid Usage Profile ID", id);
      return false;
    }

    this._usageProfileID = id;
    if (AppConstants.platform != "android") {
      Glean.usage.profileId.set(id);
    }

    Services.prefs.setStringPref(
      PREF_CACHED_USAGE_PROFILEID,
      this._usageProfileID
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
