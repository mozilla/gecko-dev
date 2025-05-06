/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { AsyncShutdown } from "resource://gre/modules/AsyncShutdown.sys.mjs";
import { DeferredTask } from "resource://gre/modules/DeferredTask.sys.mjs";

import { TelemetryUtils } from "resource://gre/modules/TelemetryUtils.sys.mjs";
import { TelemetryControllerBase } from "resource://gre/modules/TelemetryControllerBase.sys.mjs";

const Utils = TelemetryUtils;

const PING_FORMAT_VERSION = 4;

// Delay before intializing telemetry (ms)
const TELEMETRY_DELAY =
  Services.prefs.getIntPref("toolkit.telemetry.initDelay", 60) * 1000;
// Delay before initializing telemetry if we're testing (ms)
const TELEMETRY_TEST_DELAY = 1;

// How long to wait (ms) before sending the new profile ping on the first
// run of a new profile.
const NEWPROFILE_PING_DEFAULT_DELAY = 30 * 60 * 1000;

// Ping types.
const PING_TYPE_MAIN = "main";
const PING_TYPE_DELETION_REQUEST = "deletion-request";
const PING_TYPE_UNINSTALL = "uninstall";

// Session ping reasons.
const REASON_GATHER_PAYLOAD = "gather-payload";
const REASON_GATHER_SUBSESSION_PAYLOAD = "gather-subsession-payload";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ClientID: "resource://gre/modules/ClientID.sys.mjs",
  CoveragePing: "resource://gre/modules/CoveragePing.sys.mjs",
  TelemetryArchive: "resource://gre/modules/TelemetryArchive.sys.mjs",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
  TelemetryEventPing: "resource://gre/modules/EventPing.sys.mjs",
  TelemetryHealthPing: "resource://gre/modules/HealthPing.sys.mjs",
  TelemetryModules: "resource://gre/modules/ModulesPing.sys.mjs",
  TelemetryReportingPolicy:
    "resource://gre/modules/TelemetryReportingPolicy.sys.mjs",
  TelemetrySend: "resource://gre/modules/TelemetrySend.sys.mjs",
  TelemetrySession: "resource://gre/modules/TelemetrySession.sys.mjs",
  TelemetryStorage: "resource://gre/modules/TelemetryStorage.sys.mjs",
  TelemetryUntrustedModulesPing:
    "resource://gre/modules/UntrustedModulesPing.sys.mjs",
  UninstallPing: "resource://gre/modules/UninstallPing.sys.mjs",
  UpdatePing: "resource://gre/modules/UpdatePing.sys.mjs",
  UsageReporting: "resource://gre/modules/UsageReporting.sys.mjs",
});

if (
  AppConstants.platform === "win" &&
  AppConstants.MOZ_APP_NAME !== "thunderbird"
) {
  ChromeUtils.defineESModuleGetters(lazy, {
    // eslint-disable-next-line mozilla/no-browser-refs-in-toolkit
    BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.sys.mjs",
  });
}

/**
 * This is a policy object used to override behavior for testing.
 */
export var Policy = {
  now: () => new Date(),
  generatePingId: () => Utils.generateUUID(),
  getCachedClientID: () => lazy.ClientID.getCachedClientID(),
};

export var TelemetryController = Object.freeze({
  /**
   * Used only for testing purposes.
   */
  testInitLogging() {
    TelemetryControllerBase.configureLogging();
  },

  /**
   * Used only for testing purposes.
   */
  testReset() {
    return Impl.reset();
  },

  /**
   * Used only for testing purposes.
   */
  testSetup() {
    return Impl.setupTelemetry(true);
  },

  /**
   * Used only for testing purposes.
   */
  testShutdown() {
    return Impl.shutdown();
  },

  /**
   * Used only for testing purposes.
   */
  testPromiseJsProbeRegistration() {
    return Promise.resolve(Impl._probeRegistrationPromise);
  },

  /**
   * Register 'dynamic builtin' probes from the JSON definition files.
   * This is needed to support adding new probes in developer builds
   * without rebuilding the whole codebase.
   *
   * This is not meant to be used outside of local developer builds.
   */
  testRegisterJsProbes() {
    return Impl.registerJsProbes();
  },

  /**
   * Used only for testing purposes.
   */
  testPromiseDeletionRequestPingSubmitted() {
    return Promise.resolve(Impl._deletionRequestPingSubmittedPromise);
  },

  /**
   * Send a notification.
   */
  observe(aSubject, aTopic, aData) {
    return Impl.observe(aSubject, aTopic, aData);
  },

  /**
   * Submit ping payloads to Telemetry. This will assemble a complete ping, adding
   * environment data, client id and some general info.
   * Depending on configuration, the ping will be sent to the server (immediately or later)
   * and archived locally.
   *
   * To identify the different pings and to be able to query them pings have a type.
   * A type is a string identifier that should be unique to the type ping that is being submitted,
   * it should only contain alphanumeric characters and '-' for separation, i.e. satisfy:
   * /^[a-z0-9][a-z0-9-]+[a-z0-9]$/i
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {Boolean} [aOptions.usePingSender=false] if true, send the ping using the PingSender.
   * @param {String} [aOptions.overrideClientId=undefined] if set, override the
   *                 client id to the provided value. Implies aOptions.addClientId=true.
   * @param {String} [aOptions.overrideProfileGroupId=undefined] if set, override the
   *                 profile group id to the provided value. Implies aOptions.addClientId=true.
   * @returns {Promise} Test-only - a promise that resolves with the ping id once the ping is stored or sent.
   */
  submitExternalPing(aType, aPayload, aOptions = {}) {
    aOptions.addClientId = aOptions.addClientId || false;
    aOptions.addEnvironment = aOptions.addEnvironment || false;
    aOptions.usePingSender = aOptions.usePingSender || false;

    return Impl.submitExternalPing(aType, aPayload, aOptions);
  },

  /**
   * Get the current session ping data as it would be sent out or stored.
   *
   * @param {bool} aSubsession Whether to get subsession data. Optional, defaults to false.
   * @return {object} The current ping data if Telemetry is enabled, null otherwise.
   */
  getCurrentPingData(aSubsession = false) {
    return Impl.getCurrentPingData(aSubsession);
  },

  /**
   * Save a ping to disk.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Boolean} [aOptions.overwrite=false] true overwrites a ping with the same name,
   *                  if found.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {String} [aOptions.overrideClientId=undefined] if set, override the
   *                 client id to the provided value. Implies aOptions.addClientId=true.
   * @param {String} [aOptions.overrideProfileGroupId=undefined] if set, override the
   *                 profile group id to the provided value. Implies aOptions.addClientId=true.
   *
   * @returns {Promise} A promise that resolves with the ping id when the ping is saved to
   *                    disk.
   */
  addPendingPing(aType, aPayload, aOptions = {}) {
    let options = aOptions;
    options.addClientId = aOptions.addClientId || false;
    options.addEnvironment = aOptions.addEnvironment || false;
    options.overwrite = aOptions.overwrite || false;

    return Impl.addPendingPing(aType, aPayload, options);
  },

  /**
   * Check if we have an aborted-session ping from a previous session.
   * If so, submit and then remove it.
   *
   * @return {Promise} Promise that is resolved when the ping is saved.
   */
  checkAbortedSessionPing() {
    return Impl.checkAbortedSessionPing();
  },

  /**
   * Save an aborted-session ping to disk without adding it to the pending pings.
   *
   * @param {Object} aPayload The ping payload data.
   * @return {Promise} Promise that is resolved when the ping is saved.
   */
  saveAbortedSessionPing(aPayload) {
    return Impl.saveAbortedSessionPing(aPayload);
  },

  /**
   * Remove the aborted-session ping if any exists.
   *
   * @return {Promise} Promise that is resolved when the ping was removed.
   */
  removeAbortedSessionPing() {
    return Impl.removeAbortedSessionPing();
  },

  /**
   * Create an uninstall ping and write it to disk, replacing any already present.
   * This is stored independently from other pings, and only read by
   * the Windows uninstaller.
   *
   * WINDOWS ONLY, does nothing and resolves immediately on other platforms.
   *
   * @return {Promise} Resolved when the ping has been saved.
   */
  saveUninstallPing() {
    return Impl.saveUninstallPing();
  },

  /**
   * Allows the sync ping to tell the controller that it is initializing, so
   * should be included in the orderly shutdown process.
   *
   * @param {Function} aFnShutdown The function to call as telemetry shuts down.

   */
  registerSyncPingShutdown(afnShutdown) {
    Impl.registerSyncPingShutdown(afnShutdown);
  },

  /**
   * Allows waiting for TelemetryControllers delayed initialization to complete.
   * The returned promise is guaranteed to resolve before TelemetryController is shutting down.
   * @return {Promise} Resolved when delayed TelemetryController initialization completed.
   */
  promiseInitialized() {
    return Impl.promiseInitialized();
  },

  /**
   * Allows to trigger TelemetryControllers delayed initialization now and waiting for its completion.
   * The returned promise is guaranteed to resolve before TelemetryController is shutting down.
   * @return {Promise} Resolved when delayed TelemetryController initialization completed.
   */
  ensureInitialized() {
    return Impl.ensureInitialized();
  },
});

var Impl = {
  _initialized: false,
  _initStarted: false, // Whether we started setting up TelemetryController.
  _shuttingDown: false, // Whether the browser is shutting down.
  _shutDown: false, // Whether the browser has shut down.
  _logger: null,
  _prevValues: {},
  // The previous build ID, if this is the first run with a new build.
  // Undefined if this is not the first run, or the previous build ID is unknown.
  _previousBuildID: undefined,
  _clientID: null,
  _profileGroupID: null,
  // A task performing delayed initialization
  _delayedInitTask: null,
  // The deferred promise resolved when the initialization task completes.
  _delayedInitTaskDeferred: null,

  // This is a public barrier Telemetry clients can use to add blockers to the shutdown
  // of TelemetryController.
  // After this barrier, clients can not submit Telemetry pings anymore.
  _shutdownBarrier: new AsyncShutdown.Barrier(
    "TelemetryController: Waiting for clients."
  ),
  // This state is included in the async shutdown annotation for crash pings and reports.
  _shutdownState: "Shutdown not started.",
  // This is a private barrier blocked by pending async ping activity (sending & saving).
  _connectionsBarrier: new AsyncShutdown.Barrier(
    "TelemetryController: Waiting for pending ping activity"
  ),
  // This is true when running in the test infrastructure.
  _testMode: false,
  // The task performing the delayed sending of the "new-profile" ping.
  _delayedNewPingTask: null,
  // The promise used to wait for the JS probe registration (dynamic builtin).
  _probeRegistrationPromise: null,
  // The promise of any outstanding task sending the "deletion-request" ping.
  _deletionRequestPingSubmittedPromise: null,
  // A function to shutdown the sync/fxa ping, or null if that ping has not
  // self-initialized.
  _fnSyncPingShutdown: null,

  get _log() {
    return TelemetryControllerBase.log;
  },

  /**
   * Get the data for the "application" section of the ping.
   */
  _getApplicationSection() {
    // Querying architecture and update channel can throw. Make sure to recover and null
    // those fields.
    let arch = null;
    try {
      arch = Services.sysinfo.get("arch");
    } catch (e) {
      this._log.trace(
        "_getApplicationSection - Unable to get system architecture.",
        e
      );
    }

    let updateChannel = null;
    try {
      updateChannel = Utils.getUpdateChannel();
    } catch (e) {
      this._log.trace(
        "_getApplicationSection - Unable to get update channel.",
        e
      );
    }

    return {
      architecture: arch,
      buildId: Services.appinfo.appBuildID,
      name: Services.appinfo.name,
      version: Services.appinfo.version,
      displayVersion: AppConstants.MOZ_APP_VERSION_DISPLAY,
      vendor: Services.appinfo.vendor,
      platformVersion: Services.appinfo.platformVersion,
      xpcomAbi: Services.appinfo.XPCOMABI,
      channel: updateChannel,
    };
  },

  /**
   * Assemble a complete ping following the common ping format specification.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} aOptions Options object.
   * @param {Boolean} aOptions.addClientId true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} aOptions.addEnvironment true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {String} [aOptions.overrideClientId=undefined] if set, override the
   *                 client id to the provided value. Implies aOptions.addClientId=true.
   * @param {String} [aOptions.overrideProfileGroupId=undefined] if set, override the
   *                 profile group id to the provided value. Implies aOptions.addClientId=true.
   * @returns {Object} An object that contains the assembled ping data.
   */
  assemblePing: function assemblePing(aType, aPayload, aOptions = {}) {
    this._log.trace(
      "assemblePing - Type " + aType + ", aOptions " + JSON.stringify(aOptions)
    );

    // Clone the payload data so we don't race against unexpected changes in subobjects that are
    // still referenced by other code.
    // We can't trust all callers to do this properly on their own.
    let payload = Cu.cloneInto(aPayload, {});

    // Fill the common ping fields.
    let pingData = {
      type: aType,
      id: Policy.generatePingId(),
      creationDate: Policy.now().toISOString(),
      version: PING_FORMAT_VERSION,
      application: this._getApplicationSection(),
      payload,
    };

    if (
      aOptions.addClientId ||
      aOptions.overrideClientId ||
      aOptions.overrideProfileGroupId
    ) {
      pingData.clientId = aOptions.overrideClientId ?? this._clientID;
      pingData.profileGroupId =
        aOptions.overrideProfileGroupId ?? this._profileGroupID;
    }

    if (aOptions.addEnvironment) {
      pingData.environment =
        aOptions.overrideEnvironment ||
        lazy.TelemetryEnvironment.currentEnvironment;
    }

    return pingData;
  },

  /**
   * Track any pending ping send and save tasks through the promise passed here.
   * This is needed to block shutdown on any outstanding ping activity.
   */
  _trackPendingPingTask(aPromise) {
    this._connectionsBarrier.client.addBlocker(
      "Waiting for ping task",
      aPromise
    );
  },

  /**
   * Internal function to assemble a complete ping, adding environment data, client id
   * and some general info. This waits on the client id to be loaded/generated if it's
   * not yet available. Note that this function is synchronous unless we need to load
   * the client id.
   * Depending on configuration, the ping will be sent to the server (immediately or later)
   * and archived locally.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {Boolean} [aOptions.usePingSender=false] if true, send the ping using the PingSender.
   * @param {String} [aOptions.overrideClientId=undefined] if set, override the
   *                 client id to the provided value. Implies aOptions.addClientId=true.
   * @param {String} [aOptions.overrideProfileGroupId=undefined] if set, override the
   *                 profile group id to the provided value. Implies aOptions.addClientId=true.
   * @returns {Promise} Test-only - a promise that is resolved with the ping id once the ping is stored or sent.
   */
  async _submitPingLogic(aType, aPayload, aOptions) {
    // Make sure to have a clientId if we need one. This cover the case of submitting
    // a ping early during startup, before Telemetry is initialized, if no client id was
    // cached.
    let needsIdentifiers =
      aOptions.addClientId ||
      aOptions.overrideClientId ||
      aOptions.overrideProfileGroupId;
    let hasClientId = aOptions.overrideClientId ?? this._clientID;
    let hasProfileGroupId =
      aOptions.overrideProfileGroupId ?? this._profileGroupID;

    if (needsIdentifiers && !(hasClientId && hasProfileGroupId)) {
      this._log.trace(
        "_submitPingLogic - Waiting on client id or profile group id"
      );
      Glean.telemetry.pingSubmissionWaitingClientid.add(1);
      // We can safely call |getClientID| here and during initialization: we would still
      // spawn and return one single loading task.
      this._clientID = await lazy.ClientID.getClientID();
      this._profileGroupID = await lazy.ClientID.getProfileGroupID();
    }

    let pingData = this.assemblePing(aType, aPayload, aOptions);
    this._log.trace("submitExternalPing - ping assembled, id: " + pingData.id);

    // Always persist the pings if we are allowed to. We should not yield on any of the
    // following operations to keep this function synchronous for the majority of the calls.
    let archivePromise = lazy.TelemetryArchive.promiseArchivePing(
      pingData
    ).catch(e =>
      this._log.error(
        "submitExternalPing - Failed to archive ping " + pingData.id,
        e
      )
    );
    let p = [archivePromise];

    p.push(
      lazy.TelemetrySend.submitPing(pingData, {
        usePingSender: aOptions.usePingSender,
      })
    );

    return Promise.all(p).then(() => pingData.id);
  },

  /**
   * Submit ping payloads to Telemetry.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {Boolean} [aOptions.usePingSender=false] if true, send the ping using the PingSender.
   * @param {String} [aOptions.overrideClientId=undefined] if set, override the
   *                 client id to the provided value. Implies aOptions.addClientId=true.
   * @param {String} [aOptions.overrideProfileGroupId=undefined] if set, override the
   *                 profile group id to the provided value. Implies aOptions.addClientId=true.
   * @returns {Promise} Test-only - a promise that is resolved with the ping id once the ping is stored or sent.
   */
  submitExternalPing: function send(aType, aPayload, aOptions) {
    this._log.trace(
      "submitExternalPing - type: " +
        aType +
        ", aOptions: " +
        JSON.stringify(aOptions)
    );

    // Reject pings sent after shutdown.
    if (this._shutDown) {
      const errorMessage =
        "submitExternalPing - Submission is not allowed after shutdown, discarding ping of type: " +
        aType;
      this._log.error(errorMessage);
      return Promise.reject(new Error(errorMessage));
    }

    // Enforce the type string to only contain sane characters.
    const typeUuid = /^[a-z0-9][a-z0-9-]+[a-z0-9]$/i;
    if (!typeUuid.test(aType)) {
      this._log.error("submitExternalPing - invalid ping type: " + aType);
      Glean.telemetry.invalidPingTypeSubmitted[aType].add(1);
      return Promise.reject(new Error("Invalid type string submitted."));
    }
    // Enforce that the payload is an object.
    if (
      aPayload === null ||
      typeof aPayload !== "object" ||
      Array.isArray(aPayload)
    ) {
      this._log.error(
        "submitExternalPing - invalid payload type: " + typeof aPayload
      );
      Glean.telemetry.invalidPayloadSubmitted.add(1);
      return Promise.reject(new Error("Invalid payload type submitted."));
    }

    let promise = this._submitPingLogic(aType, aPayload, aOptions);
    this._trackPendingPingTask(promise);
    return promise;
  },

  /**
   * Save a ping to disk.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} aOptions Options object.
   * @param {Boolean} aOptions.addClientId true if the ping should contain the client id,
   *                  false otherwise.
   * @param {Boolean} aOptions.addEnvironment true if the ping should contain the
   *                  environment data.
   * @param {Boolean} aOptions.overwrite true overwrites a ping with the same name, if found.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {String} [aOptions.overrideClientId=undefined] if set, override the
   *                 client id to the provided value. Implies aOptions.addClientId=true.
   * @param {String} [aOptions.overrideProfileGroupId=undefined] if set, override the
   *                 profile group id to the provided value. Implies aOptions.addClientId=true.
   *
   * @returns {Promise} A promise that resolves with the ping id when the ping is saved to
   *                    disk.
   */
  addPendingPing: function addPendingPing(aType, aPayload, aOptions) {
    this._log.trace(
      "addPendingPing - Type " +
        aType +
        ", aOptions " +
        JSON.stringify(aOptions)
    );

    let pingData = this.assemblePing(aType, aPayload, aOptions);

    let savePromise = lazy.TelemetryStorage.savePendingPing(pingData);
    let archivePromise = lazy.TelemetryArchive.promiseArchivePing(
      pingData
    ).catch(e => {
      this._log.error(
        "addPendingPing - Failed to archive ping " + pingData.id,
        e
      );
    });

    // Wait for both the archiving and ping persistence to complete.
    let promises = [savePromise, archivePromise];
    return Promise.all(promises).then(() => pingData.id);
  },

  /**
   * Check whether we have an aborted-session ping. If so add it to the pending pings and archive it.
   *
   * @return {Promise} Promise that is resolved when the ping is submitted and archived.
   */
  async checkAbortedSessionPing() {
    let ping = await lazy.TelemetryStorage.loadAbortedSessionPing();
    this._log.trace(
      "checkAbortedSessionPing - found aborted-session ping: " + !!ping
    );
    if (!ping) {
      return;
    }

    try {
      // Previous aborted-session might have been with a canary client ID.
      // Don't send it.
      if (ping.clientId != Utils.knownClientID) {
        await lazy.TelemetryStorage.savePendingPing(ping);
        await lazy.TelemetryArchive.promiseArchivePing(ping);
      }
    } catch (e) {
      this._log.error(
        "checkAbortedSessionPing - Unable to add the pending ping",
        e
      );
    } finally {
      await lazy.TelemetryStorage.removeAbortedSessionPing();
    }
  },

  /**
   * Save an aborted-session ping to disk without adding it to the pending pings.
   *
   * @param {Object} aPayload The ping payload data.
   * @return {Promise} Promise that is resolved when the ping is saved.
   */
  saveAbortedSessionPing(aPayload) {
    this._log.trace("saveAbortedSessionPing");
    const options = { addClientId: true, addEnvironment: true };
    const pingData = this.assemblePing(PING_TYPE_MAIN, aPayload, options);
    return lazy.TelemetryStorage.saveAbortedSessionPing(pingData);
  },

  removeAbortedSessionPing() {
    return lazy.TelemetryStorage.removeAbortedSessionPing();
  },

  async saveUninstallPing() {
    if (AppConstants.platform != "win") {
      return undefined;
    }

    this._log.trace("saveUninstallPing");

    let payload = {};
    try {
      payload.otherInstalls = lazy.UninstallPing.getOtherInstallsCount();
      this._log.info(
        "saveUninstallPing - otherInstalls",
        payload.otherInstalls
      );
    } catch (e) {
      this._log.warn("saveUninstallPing - getOtherInstallCount failed", e);
    }
    const options = { addClientId: true, addEnvironment: true };
    const pingData = this.assemblePing(PING_TYPE_UNINSTALL, payload, options);

    return lazy.TelemetryStorage.saveUninstallPing(pingData);
  },

  /**
   * This triggers basic telemetry initialization and schedules a full initialized for later
   * for performance reasons.
   *
   * This delayed initialization means TelemetryController init can be in the following states:
   * 1) setupTelemetry was never called
   * or it was called and
   *   2) _delayedInitTask was scheduled, but didn't run yet.
   *   3) _delayedInitTask is currently running.
   *   4) _delayedInitTask finished running and is nulled out.
   *
   * @return {Promise} Resolved when TelemetryController and TelemetrySession are fully
   *                   initialized. This is only used in tests.
   */
  setupTelemetry: function setupTelemetry(testing) {
    this._initStarted = true;
    this._shuttingDown = false;
    this._shutDown = false;
    this._testMode = testing;

    this._log.trace("setupTelemetry");

    if (this._delayedInitTask) {
      this._log.error("setupTelemetry - init task already running");
      return this._delayedInitTaskDeferred.promise;
    }

    if (this._initialized && !this._testMode) {
      this._log.error("setupTelemetry - already initialized");
      return Promise.resolve();
    }

    // Enable adding scalars in artifact builds and build faster modes.
    // The function is async: we intentionally don't wait for it to complete
    // as we don't want to delay startup.
    this._probeRegistrationPromise = this.registerJsProbes();

    // This will trigger displaying the datachoices infobar.
    lazy.TelemetryReportingPolicy.setup();

    if (!TelemetryControllerBase.enableTelemetryRecording()) {
      this._log.config(
        "setupChromeProcess - Telemetry recording is disabled, skipping Chrome process setup."
      );
      return Promise.resolve();
    }

    this._attachObservers();

    // Perform a lightweight, early initialization for the component, just registering
    // a few observers and initializing the session.
    lazy.TelemetrySession.earlyInit(this._testMode);
    Services.telemetry.earlyInit();

    // Annotate crash reports so that we get pings for startup crashes
    lazy.TelemetrySend.earlyInit();

    // For very short session durations, we may never load the client
    // id from disk.
    // We try to cache it in prefs to avoid this, even though this may
    // lead to some stale client ids.
    this._clientID = lazy.ClientID.getCachedClientID();
    this._profileGroupID = lazy.ClientID.getCachedProfileGroupID();

    // Init the update ping telemetry as early as possible. This won't have
    // an impact on startup.
    lazy.UpdatePing.earlyInit();

    // Delay full telemetry initialization to give the browser time to
    // run various late initializers. Otherwise our gathered memory
    // footprint and other numbers would be too optimistic.
    this._delayedInitTaskDeferred = Promise.withResolvers();
    this._delayedInitTask = new DeferredTask(
      async () => {
        try {
          // TODO: This should probably happen after all the delayed init here.
          this._initialized = true;
          await lazy.TelemetryEnvironment.delayedInit();

          // Load the ClientID.
          this._clientID = await lazy.ClientID.getClientID();
          this._profileGroupID = await lazy.ClientID.getProfileGroupID();

          // Fix-up a canary client ID if detected.
          const uploadEnabled = Services.prefs.getBoolPref(
            TelemetryUtils.Preferences.FhrUploadEnabled,
            false
          );
          if (
            uploadEnabled &&
            (this._clientID == Utils.knownClientID ||
              this._profileGroupID == Utils.knownProfileGroupID)
          ) {
            this._log.trace(
              "Upload enabled, but got canary identifiers. Resetting."
            );
            await lazy.ClientID.resetIdentifiers();
            this._clientID = await lazy.ClientID.getClientID();
            this._profileGroupID = await lazy.ClientID.getProfileGroupID();
          } else if (
            !uploadEnabled &&
            (this._clientID != Utils.knownClientID ||
              this._profileGroupID != Utils.knownProfileGroupID)
          ) {
            this._log.trace(
              "Upload disabled, but got a valid client ID. Setting canary client ID."
            );
            await lazy.ClientID.setCanaryIdentifiers();
            this._clientID = await lazy.ClientID.getClientID();
            this._profileGroupID = await lazy.ClientID.getProfileGroupID();
          }

          await lazy.TelemetrySend.setup(this._testMode);

          // Perform TelemetrySession delayed init.
          await lazy.TelemetrySession.delayedInit();
          await Services.telemetry.delayedInit();

          if (
            Services.prefs.getBoolPref(
              TelemetryUtils.Preferences.NewProfilePingEnabled,
              false
            ) &&
            !lazy.TelemetrySession.newProfilePingSent
          ) {
            // Kick off the scheduling of the new-profile ping.
            this.scheduleNewProfilePing();
          }

          // Purge the pings archive by removing outdated pings. We don't wait for
          // this task to complete, but TelemetryStorage blocks on it during
          // shutdown.
          lazy.TelemetryStorage.runCleanPingArchiveTask();

          // Now that FHR/healthreporter is gone, make sure to remove FHR's DB from
          // the profile directory. This is a temporary measure that we should drop
          // in the future.
          lazy.TelemetryStorage.removeFHRDatabase();

          // The init sequence is forced to run on shutdown for short sessions and
          // we don't want to start TelemetryModules as the timer registration will fail.
          if (!this._shuttingDown) {
            // Report the modules loaded in the Firefox process.
            lazy.TelemetryModules.start();

            // Send coverage ping.
            await lazy.CoveragePing.startup();

            // Start the untrusted modules ping, which reports events where
            // untrusted modules were loaded into the Firefox process.
            if (AppConstants.platform == "win") {
              lazy.TelemetryUntrustedModulesPing.start();
            }
          }

          lazy.TelemetryEventPing.startup();

          if (uploadEnabled) {
            await this.saveUninstallPing().catch(e =>
              this._log.warn("_delayedInitTask - saveUninstallPing failed", e)
            );
          } else {
            await lazy.TelemetryStorage.removeUninstallPings().catch(e =>
              this._log.warn("_delayedInitTask - saveUninstallPing", e)
            );
          }

          this._delayedInitTaskDeferred.resolve();
        } catch (e) {
          this._delayedInitTaskDeferred.reject(e);
        } finally {
          this._delayedInitTask = null;
        }
      },
      this._testMode ? TELEMETRY_TEST_DELAY : TELEMETRY_DELAY,
      this._testMode ? 0 : undefined
    );

    IOUtils.sendTelemetry.addBlocker(
      "TelemetryController: shutting down",
      () => this.shutdown(),
      () => this._getState()
    );

    this._delayedInitTask.arm();
    return this._delayedInitTaskDeferred.promise;
  },

  // Do proper shutdown waiting and cleanup.
  async _cleanupOnShutdown() {
    if (!this._initialized) {
      return;
    }

    this._detachObservers();

    // Now do an orderly shutdown.
    try {
      if (this._delayedNewPingTask) {
        await this._delayedNewPingTask.finalize();
      }

      lazy.UpdatePing.shutdown();

      lazy.TelemetryEventPing.shutdown();

      // Shutdown the sync ping if it is initialized - this is likely, but not
      // guaranteed, to submit a "shutdown" sync ping.
      if (this._fnSyncPingShutdown) {
        this._fnSyncPingShutdown();
      }

      // Stop the datachoices infobar display.
      lazy.TelemetryReportingPolicy.shutdown();
      lazy.TelemetryEnvironment.shutdown();

      // Stop any ping sending.
      await lazy.TelemetrySend.shutdown();

      // Send latest data.
      await lazy.TelemetryHealthPing.shutdown();

      await lazy.TelemetrySession.shutdown();
      await Services.telemetry.shutdown();

      // First wait for clients processing shutdown.
      await this._shutdownBarrier.wait();

      // ... and wait for any outstanding async ping activity.
      await this._connectionsBarrier.wait();

      if (AppConstants.platform !== "android") {
        // No PingSender on Android.
        lazy.TelemetrySend.flushPingSenderBatch();
      }

      // Perform final shutdown operations.
      await lazy.TelemetryStorage.shutdown();
    } finally {
      // Reset state.
      this._initialized = false;
      this._initStarted = false;
      this._shutDown = true;
    }
  },

  shutdown() {
    this._log.trace("shutdown");

    this._shuttingDown = true;

    // We can be in one the following states here:
    // 1) setupTelemetry was never called
    // or it was called and
    //   2) _delayedInitTask was scheduled, but didn't run yet.
    //   3) _delayedInitTask is running now.
    //   4) _delayedInitTask finished running already.

    // This handles 1).
    if (!this._initStarted) {
      this._shutDown = true;
      return Promise.resolve();
    }

    // This handles 4).
    if (!this._delayedInitTask) {
      // We already ran the delayed initialization.
      return this._cleanupOnShutdown();
    }

    // This handles 2) and 3).
    return this._delayedInitTask
      .finalize()
      .then(() => this._cleanupOnShutdown());
  },

  /**
   * This observer drives telemetry.
   */
  observe(aSubject, aTopic, aData) {
    // The logger might still be not available at this point.
    if (aTopic == "profile-after-change") {
      // If we don't have a logger, we need to make sure |Log.repository.getLogger()| is
      // called before |getLoggerWithMessagePrefix|. Otherwise logging won't work.
      TelemetryControllerBase.configureLogging();
    }

    this._log.trace(`observe - ${aTopic} notified.`);

    switch (aTopic) {
      case "profile-after-change":
        // profile-after-change is only registered for chrome processes.
        return this.setupTelemetry();
      case "nsPref:changed":
        if (aData == TelemetryUtils.Preferences.FhrUploadEnabled) {
          return this._onUploadPrefChange();
        }
        if (aData == "datareporting.usage.uploadEnabled") {
          return lazy.UsageReporting._onUsagePrefChange();
        }
    }
    return undefined;
  },

  /**
   * Register the sync ping's shutdown handler.
   */
  registerSyncPingShutdown(fnShutdown) {
    if (this._fnSyncPingShutdown) {
      throw new Error("The sync ping shutdown handler is already registered.");
    }
    this._fnSyncPingShutdown = fnShutdown;
  },

  /**
   * Get an object describing the current state of this module for AsyncShutdown diagnostics.
   */
  _getState() {
    return {
      initialized: this._initialized,
      initStarted: this._initStarted,
      haveDelayedInitTask: !!this._delayedInitTask,
      shutdownBarrier: this._shutdownBarrier.state,
      connectionsBarrier: this._connectionsBarrier.state,
      sendModule: lazy.TelemetrySend.getShutdownState(),
      haveDelayedNewProfileTask: !!this._delayedNewPingTask,
    };
  },

  /**
   * Called whenever the FHR Upload preference changes (e.g. when user disables FHR from
   * the preferences panel), this triggers sending the "deletion-request" ping.
   */
  _onUploadPrefChange() {
    const uploadEnabled = Services.prefs.getBoolPref(
      TelemetryUtils.Preferences.FhrUploadEnabled,
      false
    );
    if (uploadEnabled) {
      this._log.trace(
        "_onUploadPrefChange - upload was enabled again. Resetting identifiers"
      );

      // Delete cached identifiers immediately, so other usage is forced to refetch it.
      this._clientID = null;
      this._profileGroupID = null;

      // Generate a new client ID and make sure this module uses the new version
      let p = (async () => {
        await lazy.ClientID.resetIdentifiers();
        // For the time being this is tied to the telemetry upload preference.
        await lazy.ClientID.resetUsageProfileIdentifiers();
        this._clientID = await lazy.ClientID.getClientID();
        this._profileGroupID = await lazy.ClientID.getProfileGroupID();
        Glean.telemetry.dataUploadOptin.set(true);

        await this.saveUninstallPing().catch(e =>
          this._log.warn("_onUploadPrefChange - saveUninstallPing failed", e)
        );
      })();

      this._shutdownBarrier.client.addBlocker(
        "TelemetryController: resetting client ID after data upload was enabled",
        p
      );

      return;
    }

    let p = (async () => {
      try {
        // 1. Cancel the current pings.
        // 2. Clear unpersisted pings
        await lazy.TelemetrySend.clearCurrentPings();

        // 3. Remove all pending pings
        await lazy.TelemetryStorage.removeAppDataPings();
        await lazy.TelemetryStorage.runRemovePendingPingsTask();
        await lazy.TelemetryStorage.removeUninstallPings();
      } catch (e) {
        this._log.error(
          "_onUploadPrefChange - error clearing pending pings",
          e
        );
      } finally {
        // 4. Reset session and subsession counter
        lazy.TelemetrySession.resetSubsessionCounter();

        // 5. Collect any additional identifiers we want to send in the
        // deletion request.
        const scalars = Services.telemetry.getSnapshotForScalars(
          "deletion-request",
          /* clear */ true
        );

        // 6. Set identifiers to a known values
        let oldClientId = await lazy.ClientID.getClientID();
        let oldProfileGroupId = await lazy.ClientID.getProfileGroupID();
        await lazy.ClientID.setCanaryIdentifiers();
        this._clientID = await lazy.ClientID.getClientID();
        this._profileGroupID = await lazy.ClientID.getProfileGroupID();

        // 7. Send the deletion-request ping.
        this._log.trace("_onUploadPrefChange - Sending deletion-request ping.");
        this.submitExternalPing(
          PING_TYPE_DELETION_REQUEST,
          { scalars },
          {
            overrideClientId: oldClientId,
            overrideProfileGroupId: oldProfileGroupId,
          }
        );
        this._deletionRequestPingSubmittedPromise = null;
      }
    })();

    this._deletionRequestPingSubmittedPromise = p;
    this._shutdownBarrier.client.addBlocker(
      "TelemetryController: removing pending pings after data upload was disabled",
      p
    );

    Services.obs.notifyObservers(
      null,
      TelemetryUtils.TELEMETRY_UPLOAD_DISABLED_TOPIC
    );
  },

  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  _attachObservers() {
    if (TelemetryControllerBase.IS_UNIFIED_TELEMETRY) {
      // Watch the FHR upload setting to trigger "deletion-request" pings.
      Services.prefs.addObserver(
        TelemetryUtils.Preferences.FhrUploadEnabled,
        this
      );
    }
    if (AppConstants.MOZ_APP_NAME == "firefox") {
      // Firefox-only: watch the usage reporting setting to enable, disable, and
      // trigger "usage-deletion-request" pings.
      Services.prefs.addObserver("datareporting.usage.uploadEnabled", this);
    }
  },

  /**
   * Remove the preference observer to avoid leaks.
   */
  _detachObservers() {
    if (TelemetryControllerBase.IS_UNIFIED_TELEMETRY) {
      Services.prefs.removeObserver(
        TelemetryUtils.Preferences.FhrUploadEnabled,
        this
      );
    }
    if (AppConstants.MOZ_APP_NAME == "firefox") {
      Services.prefs.removeObserver("datareporting.usage.uploadEnabled", this);
    }
  },

  /**
   * Allows waiting for TelemetryControllers delayed initialization to complete.
   * This will complete before TelemetryController is shutting down.
   * @return {Promise} Resolved when delayed TelemetryController initialization completed.
   */
  promiseInitialized() {
    return this._delayedInitTaskDeferred.promise;
  },

  /**
   * Allows to trigger TelemetryControllers delayed initialization now and waiting for its completion.
   * This will complete before TelemetryController is shutting down.
   * @return {Promise} Resolved when delayed TelemetryController initialization completed.
   */
  ensureInitialized() {
    if (this._delayedInitTask) {
      return this._delayedInitTask.finalize();
    }
    return Promise.resolve();
  },

  getCurrentPingData(aSubsession) {
    this._log.trace("getCurrentPingData - subsession: " + aSubsession);

    // Telemetry is disabled, don't gather any data.
    if (!Services.telemetry.canRecordBase) {
      return null;
    }

    const reason = aSubsession
      ? REASON_GATHER_SUBSESSION_PAYLOAD
      : REASON_GATHER_PAYLOAD;
    const type = PING_TYPE_MAIN;
    const payload = lazy.TelemetrySession.getPayload(reason);
    const options = { addClientId: true, addEnvironment: true };
    const ping = this.assemblePing(type, payload, options);

    return ping;
  },

  async reset() {
    this._clientID = null;
    this._profileGroupID = null;
    this._fnSyncPingShutdown = null;
    this._detachObservers();

    let sessionReset = lazy.TelemetrySession.testReset();

    this._connectionsBarrier = new AsyncShutdown.Barrier(
      "TelemetryController: Waiting for pending ping activity"
    );
    this._shutdownBarrier = new AsyncShutdown.Barrier(
      "TelemetryController: Waiting for clients."
    );

    // We need to kick of the controller setup first for tests that check the
    // cached client id.
    let controllerSetup = this.setupTelemetry(true);

    await sessionReset;
    await lazy.TelemetrySend.reset();
    await lazy.TelemetryStorage.reset();
    await lazy.TelemetryEnvironment.testReset();

    await controllerSetup;
  },

  /**
   * Schedule sending the "new-profile" ping.
   */
  scheduleNewProfilePing() {
    this._log.trace("scheduleNewProfilePing");

    const sendDelay = Services.prefs.getIntPref(
      TelemetryUtils.Preferences.NewProfilePingDelay,
      NEWPROFILE_PING_DEFAULT_DELAY
    );

    if (
      AppConstants.platform == "win" &&
      AppConstants.MOZ_APP_NAME !== "thunderbird"
    ) {
      try {
        // This is asynchronous, but we aren't going to await on it now. Just
        // kick it off.
        lazy.BrowserUsageTelemetry.reportInstallationTelemetry();
      } catch (ex) {
        this._log.warn(
          "scheduleNewProfilePing - reportInstallationTelemetry failed",
          ex
        );
      }
    }

    this._delayedNewPingTask = new DeferredTask(async () => {
      try {
        await this.sendNewProfilePing();
      } finally {
        this._delayedNewPingTask = null;
      }
    }, sendDelay);

    this._delayedNewPingTask.arm();
  },

  /**
   * Generate and send the new-profile ping
   */
  async sendNewProfilePing() {
    this._log.trace(
      "sendNewProfilePing - shutting down: " + this._shuttingDown
    );

    if (
      AppConstants.platform == "win" &&
      AppConstants.MOZ_APP_NAME !== "thunderbird"
    ) {
      let failureReason = "UnknownError";
      try {
        await lazy.BrowserUsageTelemetry.reportInstallationTelemetry();
        failureReason = "NoError";
      } catch (ex) {
        this._log.warn(
          "sendNewProfilePing - reportInstallationTelemetry failed",
          ex
        );
        // Overwrite with a more specific error if possible.
        failureReason = ex.name;
      } finally {
        // No dataPathOverride here so we can check the default location
        // for installation_telemetry.json
        let dataPath = Services.dirsvc.get("GreD", Ci.nsIFile);
        dataPath.append("installation_telemetry.json");
        let fileExists = await IOUtils.exists(dataPath.path);
        if (!fileExists) {
          failureReason = "NotFoundError";
        }
        Glean.installationFirstSeen.failureReason.set(failureReason);
      }
    }

    const scalars = Services.telemetry.getSnapshotForScalars(
      "new-profile",
      /* clear */ true
    );

    // Generate the payload.
    const payload = {
      reason: this._shuttingDown ? "shutdown" : "startup",
      processes: {
        parent: {
          scalars: scalars.parent,
        },
      },
    };

    // Generate and send the "new-profile" ping. This uses the
    // pingsender if we're shutting down.
    let options = {
      addClientId: true,
      addEnvironment: true,
      usePingSender: this._shuttingDown,
    };
    // TODO: we need to be smarter about when to send the ping (and save the
    // state to file). |requestIdleCallback| is currently only accessible
    // through DOM. See bug 1361996.
    await TelemetryController.submitExternalPing(
      "new-profile",
      payload,
      options
    ).then(
      () => lazy.TelemetrySession.markNewProfilePingSent(),
      e =>
        this._log.error(
          "sendNewProfilePing - failed to submit new-profile ping",
          e
        )
    );
  },

  /**
   * Register 'dynamic builtin' probes from the JSON definition files.
   * This is needed to support adding new probes in developer builds
   * without rebuilding the whole codebase.
   *
   * This is not meant to be used outside of local developer builds.
   */
  async registerJsProbes() {
    // We don't support this outside of developer builds.
    if (AppConstants.MOZILLA_OFFICIAL && !this._testMode) {
      return;
    }

    this._log.trace("registerJsProbes - registering builtin JS probes");

    await this.registerScalarProbes();
    await this.registerEventProbes();
  },

  _loadProbeDefinitions(filename) {
    let probeFile = Services.dirsvc.get("GreD", Ci.nsIFile);
    probeFile.append(filename);
    if (!probeFile.exists()) {
      this._log.trace(
        `loadProbeDefinitions - no builtin JS probe file ${filename}`
      );
      return null;
    }

    return IOUtils.readUTF8(probeFile.path);
  },

  async registerScalarProbes() {
    this._log.trace(
      "registerScalarProbes - registering scalar builtin JS probes"
    );

    // Load the scalar probes JSON file.
    const scalarProbeFilename = "ScalarArtifactDefinitions.json";
    let scalarJSProbes = {};
    try {
      let fileContent = await this._loadProbeDefinitions(scalarProbeFilename);
      scalarJSProbes = JSON.parse(fileContent, (property, value) => {
        // Fixup the "kind" property: it's a string, and we need the constant
        // coming from nsITelemetry.
        if (property !== "kind" || typeof value != "string") {
          return value;
        }

        let newValue;
        switch (value) {
          case "nsITelemetry::SCALAR_TYPE_COUNT":
            newValue = Services.telemetry.SCALAR_TYPE_COUNT;
            break;
          case "nsITelemetry::SCALAR_TYPE_BOOLEAN":
            newValue = Services.telemetry.SCALAR_TYPE_BOOLEAN;
            break;
          case "nsITelemetry::SCALAR_TYPE_STRING":
            newValue = Services.telemetry.SCALAR_TYPE_STRING;
            break;
        }
        return newValue;
      });
    } catch (ex) {
      this._log.error(
        `registerScalarProbes - there was an error loading ${scalarProbeFilename}`,
        ex
      );
    }

    // Register the builtin probes.
    for (let category in scalarJSProbes) {
      // Expire the expired scalars
      for (let name in scalarJSProbes[category]) {
        let def = scalarJSProbes[category][name];
        if (
          !def ||
          !def.expires ||
          def.expires == "never" ||
          def.expires == "default"
        ) {
          continue;
        }
        if (
          Services.vc.compare(AppConstants.MOZ_APP_VERSION, def.expires) >= 0
        ) {
          def.expired = true;
        }
      }
      Services.telemetry.registerBuiltinScalars(
        category,
        scalarJSProbes[category]
      );
    }
  },

  async registerEventProbes() {
    this._log.trace(
      "registerEventProbes - registering builtin JS Event probes"
    );

    // Load the event probes JSON file.
    const eventProbeFilename = "EventArtifactDefinitions.json";
    let eventJSProbes = {};
    try {
      let fileContent = await this._loadProbeDefinitions(eventProbeFilename);
      eventJSProbes = JSON.parse(fileContent);
    } catch (ex) {
      this._log.error(
        `registerEventProbes - there was an error loading ${eventProbeFilename}`,
        ex
      );
    }

    // Register the builtin probes.
    for (let category in eventJSProbes) {
      for (let name in eventJSProbes[category]) {
        let def = eventJSProbes[category][name];
        if (
          !def ||
          !def.expires ||
          def.expires == "never" ||
          def.expires == "default"
        ) {
          continue;
        }
        if (
          Services.vc.compare(AppConstants.MOZ_APP_VERSION, def.expires) >= 0
        ) {
          def.expired = true;
        }
      }
      Services.telemetry.registerBuiltinEvents(
        category,
        eventJSProbes[category]
      );
    }
  },
};
