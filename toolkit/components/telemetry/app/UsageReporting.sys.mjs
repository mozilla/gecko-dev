/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Log } from "resource://gre/modules/Log.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "UsageReporting::";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ClientID: "resource://gre/modules/ClientID.sys.mjs",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.sys.mjs",
});

export var UsageReporting = {
  _logger: null,
  _initPromise: null,

  /**
   * Return a promise resolving when this module is initialized.
   */
  async ensureInitialized() {
    const SLUG = "ensureInitialized";

    if (this._initPromise) {
      return this._initPromise;
    }

    this._initPromise = (async () => {
      let profileID = await lazy.ClientID.getUsageProfileID();
      const uploadEnabled = Services.prefs.getBoolPref(
        "datareporting.usage.uploadEnabled",
        false
      );
      this._log.trace(
        `${SLUG}: uploadEnabled=${uploadEnabled}, profileID='${profileID}'`
      );

      // Usage deletion requests can always be sent.  They are
      // always sent in response to user action.
      GleanPings.usageDeletionRequest.setEnabled(true);

      // Usage pings should only be sent when upload is enabled.
      // Eventually, Glean will persist this setting.
      GleanPings.usageReporting.setEnabled(uploadEnabled);

      if (
        uploadEnabled &&
        profileID == lazy.TelemetryUtils.knownUsageProfileID
      ) {
        await lazy.ClientID.resetUsageProfileIdentifier();
        this._log.info(`${SLUG}: Reset usage profile identifier.`);
      } else if (
        !uploadEnabled &&
        profileID != lazy.TelemetryUtils.knownUsageProfileID
      ) {
        await lazy.ClientID.setCanaryUsageProfileIdentifier();
        this._log.info(`${SLUG}: Set canary usage profile identifier.`);
      } else {
        // Great!  We've got a consistent state.  Glean has our profile
        // identifier to include in pings, if enabled.
        this._log.trace(`${SLUG}: No usage profile identifier action taken.`);
      }
    })();
    return this._initPromise;
  },

  /**
   * Called whenever the usage reporting preference changes (e.g. when user
   * disables usage reporting from the preferences panel).  This triggers
   * sending the "usage-deletion-request" ping.
   *
   * N.b. This listener is invoked from `TelemetryControllerParent` to simplify
   * integrating with the test harnesses.
   */
  async _onUsagePrefChange() {
    if (AppConstants.MOZ_APP_NAME !== "firefox") {
      this._log.warn(
        "_onUsagePrefChange: incorrectly invoked in non-Firefox build."
      );
      return;
    }

    const uploadEnabled = Services.prefs.getBoolPref(
      "datareporting.usage.uploadEnabled",
      false
    );

    if (uploadEnabled) {
      // Rising edge: enable "usage-reporting" ping.
      this._log.trace(
        "_onUsagePrefChange: witnessed rising edge, enabling 'usage-reporting' ping, " +
          "and resetting identifier."
      );
      // Must be _after_ the usage reporting ping is enabled, or the next
      // submission may not contain the new identifier.  This call will enable
      // the ping.
      await lazy.ClientID.resetUsageProfileIdentifier();
    } else {
      // Falling edge: disable "usage-reporting" ping, send "usage-deletion-request" ping.
      this._log.trace(
        "_onUsagePrefChange: witnessed falling edge, disabling 'usage-reporting' ping, " +
          "setting canary identifier, and sending 'usage-deletion-request' ping."
      );
      GleanPings.usageDeletionRequest.submit("set_upload_enabled");

      // Must be _after_ the deletion request is sent, or the request will not
      // contain the previous identifier.  This call will disable the ping.
      await lazy.ClientID.setCanaryUsageProfileIdentifier();
    }
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
