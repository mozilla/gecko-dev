/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Log } from "resource://gre/modules/Log.sys.mjs";
import { clearTimeout, setTimeout } from "resource://gre/modules/Timer.sys.mjs";

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { Observers } from "resource://services-common/observers.sys.mjs";
import { TelemetryUtils } from "resource://gre/modules/TelemetryUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  TelemetrySend: "resource://gre/modules/TelemetrySend.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  OnboardingMessageProvider:
    "resource:///modules/asrouter/OnboardingMessageProvider.sys.mjs",
  WinTaskbarJumpList: "resource:///modules/WindowsJumpLists.sys.mjs",
});

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryReportingPolicy::";

// Oldest year to allow in date preferences. The FHR infobar was implemented in
// 2012 and no dates older than that should be encountered.
const OLDEST_ALLOWED_ACCEPTANCE_YEAR = 2012;

const PREF_BRANCH = "datareporting.policy.";

// The following preferences are deprecated and will be purged during the preferences
// migration process.
const DEPRECATED_FHR_PREFS = [
  PREF_BRANCH + "dataSubmissionPolicyAccepted",
  PREF_BRANCH + "dataSubmissionPolicyBypassAcceptance",
  PREF_BRANCH + "dataSubmissionPolicyResponseType",
  PREF_BRANCH + "dataSubmissionPolicyResponseTime",
];

// How much time until we display the data choices notification bar, on the first run.
const NOTIFICATION_DELAY_FIRST_RUN_MSEC = 60 * 1000; // 60s
// Same as above, for the next runs.
const NOTIFICATION_DELAY_NEXT_RUNS_MSEC = 10 * 1000; // 10s

/**
 * This is a policy object used to override behavior within this module.
 * Tests override properties on this object to allow for control of behavior
 * that would otherwise be very hard to cover.
 */
export var Policy = {
  now: () => new Date(),
  setShowInfobarTimeout: (callback, delayMs) => setTimeout(callback, delayMs),
  clearShowInfobarTimeout: id => clearTimeout(id),
  fakeSessionRestoreNotification: async () => {
    TelemetryReportingPolicyImpl.observe(
      null,
      "sessionstore-windows-restored",
      null
    );
  },
  showModal: async data => {
    return TelemetryReportingPolicyImpl._showModal(data);
  },
  delayedSetup: async () => TelemetryReportingPolicyImpl._delayedSetup(),
};

/**
 * Represents a request to display data policy.
 *
 * Receivers of these instances are expected to call one or more of the on*
 * functions when events occur.
 *
 * When one of these requests is received, the first thing a callee should do
 * is present notification to the user of the data policy. When the notice
 * is displayed to the user, the callee should call `onUserNotifyComplete`.
 *
 * If for whatever reason the callee could not display a notice,
 * it should call `onUserNotifyFailed`.
 *
 * @param {Object} aLog The log object used to log the error in case of failures.
 * @param {function} aResolve Promise-like callback function, invoked with
 *                            `true` (complete) or `false` (error).
 */
function NotifyPolicyRequest(aLog, aResolve) {
  this._log = aLog;
  this._resolve = aResolve;
}

NotifyPolicyRequest.prototype = Object.freeze({
  /**
   * Called when the user is notified of the policy.
   */
  onUserNotifyComplete() {
    this._resolve(true);
  },

  /**
   * Called when there was an error notifying the user about the policy.
   *
   * @param error
   *        (Error) Explains what went wrong.
   */
  onUserNotifyFailed(error) {
    this._log.error("onUserNotifyFailed - " + error);
    this._resolve(false);
  },
});

export var TelemetryReportingPolicy = {
  // The current policy version number. If the version number stored in the prefs
  // is smaller than this, data upload will be disabled until the user is re-notified
  // about the policy changes.
  DEFAULT_DATAREPORTING_POLICY_VERSION: 1,

  /**
   * Setup the policy.
   */
  setup() {
    return TelemetryReportingPolicyImpl.setup();
  },

  /**
   * Shutdown and clear the policy.
   */
  shutdown() {
    return TelemetryReportingPolicyImpl.shutdown();
  },

  /**
   * Check if we are allowed to upload data. In order to submit data both these conditions
   * should be true:
   * - The data submission preference should be true.
   * - The datachoices infobar should have been displayed.
   *
   * @return {Boolean} True if we are allowed to upload data, false otherwise.
   */
  canUpload() {
    return TelemetryReportingPolicyImpl.canUpload();
  },

  /**
   * Check if this is the first time the browser ran.
   */
  isFirstRun() {
    return TelemetryReportingPolicyImpl.isFirstRun();
  },

  /**
   * Test only method, restarts the policy.
   */
  reset() {
    return TelemetryReportingPolicyImpl.reset();
  },

  /**
   * Test only method, used to check if the policy should notify in tests.
   */
  testShouldNotify() {
    return TelemetryReportingPolicyImpl._shouldNotify();
  },

  /**
   * Test only method, used to check if user is notified of the policy in tests.
   */
  testIsUserNotified() {
    return TelemetryReportingPolicyImpl.isUserNotifiedOfCurrentPolicy;
  },

  /**
   * Test only method, used to simulate the infobar being shown in xpcshell tests.
   */
  testInfobarShown() {
    return TelemetryReportingPolicyImpl._userNotified();
  },

  /**
   * Test only method, used to trigger an update of the "first run" state.
   */
  testUpdateFirstRun() {
    TelemetryReportingPolicyImpl._isFirstRun = undefined;
    TelemetryReportingPolicyImpl.isFirstRun();
  },

  async ensureUserIsNotified() {
    return TelemetryReportingPolicyImpl.ensureUserIsNotified();
  },
};

var TelemetryReportingPolicyImpl = {
  _logger: null,
  // Keep track of the notification status if user wasn't notified already.
  _notificationInProgress: false,
  // The timer used to show the datachoices notification at startup.
  _startupNotificationTimerId: null,
  // Keep track of the first session state, as the related preference
  // is flipped right after the browser starts.
  _isFirstRun: undefined,
  // Ensure notification flow is idempotent.
  _ensureUserIsNotifiedPromise: undefined,
  // Nimbus `preonboarding` feature variables.  Set in response to
  // `sessionstore-window-restored`; immutable there-after.
  _nimbusVariables: {},

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(
        LOGGER_NAME,
        LOGGER_PREFIX
      );
    }

    return this._logger;
  },

  /**
   * Get the date the policy was notified.
   * @return {Object} A date object or null on errors.
   */
  get dataSubmissionPolicyNotifiedDate() {
    let prefString = Services.prefs.getStringPref(
      TelemetryUtils.Preferences.AcceptedPolicyDate,
      "0"
    );
    let valueInteger = parseInt(prefString, 10);

    // Bail out if we didn't store any value yet.
    if (valueInteger == 0) {
      this._log.info(
        "get dataSubmissionPolicyNotifiedDate - No date stored yet."
      );
      return null;
    }

    // If an invalid value is saved in the prefs, bail out too.
    if (Number.isNaN(valueInteger)) {
      this._log.error(
        "get dataSubmissionPolicyNotifiedDate - Invalid date stored."
      );
      return null;
    }

    // Make sure the notification date is newer then the oldest allowed date.
    let date = new Date(valueInteger);
    if (date.getFullYear() < OLDEST_ALLOWED_ACCEPTANCE_YEAR) {
      this._log.error(
        "get dataSubmissionPolicyNotifiedDate - The stored date is too old."
      );
      return null;
    }

    return date;
  },

  /**
   * Set the date the policy was notified.
   * @param {Object} aDate A valid date object.
   */
  set dataSubmissionPolicyNotifiedDate(aDate) {
    this._log.trace("set dataSubmissionPolicyNotifiedDate - aDate: " + aDate);

    if (!aDate || aDate.getFullYear() < OLDEST_ALLOWED_ACCEPTANCE_YEAR) {
      this._log.error(
        "set dataSubmissionPolicyNotifiedDate - Invalid notification date."
      );
      return;
    }

    Services.prefs.setStringPref(
      TelemetryUtils.Preferences.AcceptedPolicyDate,
      aDate.getTime().toString()
    );
  },

  /**
   * Whether submission of data is allowed.
   *
   * This is the master switch for remote server communication. If it is
   * false, we never request upload or deletion.
   */
  get dataSubmissionEnabled() {
    // Default is true because we are opt-out.
    return Services.prefs.getBoolPref(
      TelemetryUtils.Preferences.DataSubmissionEnabled,
      true
    );
  },

  get currentPolicyVersion() {
    return Services.prefs.getIntPref(
      TelemetryUtils.Preferences.CurrentPolicyVersion,
      TelemetryReportingPolicy.DEFAULT_DATAREPORTING_POLICY_VERSION
    );
  },

  /**
   * The minimum policy version which for dataSubmissionPolicyAccepted to
   * to be valid.
   */
  get minimumPolicyVersion() {
    const minPolicyVersion = Services.prefs.getIntPref(
      TelemetryUtils.Preferences.MinimumPolicyVersion,
      1
    );

    // First check if the current channel has a specific minimum policy version. If not,
    // use the general minimum policy version.
    let channel = "";
    try {
      channel = TelemetryUtils.getUpdateChannel();
    } catch (e) {
      this._log.error(
        "minimumPolicyVersion - Unable to retrieve the current channel."
      );
      return minPolicyVersion;
    }
    const channelPref =
      TelemetryUtils.Preferences.MinimumPolicyVersion + ".channel-" + channel;
    return Services.prefs.getIntPref(channelPref, minPolicyVersion);
  },

  get dataSubmissionPolicyAcceptedVersion() {
    return Services.prefs.getIntPref(
      TelemetryUtils.Preferences.AcceptedPolicyVersion,
      0
    );
  },

  set dataSubmissionPolicyAcceptedVersion(value) {
    Services.prefs.setIntPref(
      TelemetryUtils.Preferences.AcceptedPolicyVersion,
      value
    );
  },

  /**
   * Checks to see if the user has been notified about data submission
   * @return {Bool} True if user has been notified and the notification is still valid,
   *         false otherwise.
   */
  get isUserNotifiedOfCurrentPolicy() {
    // If we don't have a sane notification date, the user was not notified yet.
    if (
      !this.dataSubmissionPolicyNotifiedDate ||
      this.dataSubmissionPolicyNotifiedDate.getTime() <= 0
    ) {
      return false;
    }

    // The accepted policy version should not be less than the minimum policy version.
    if (this.dataSubmissionPolicyAcceptedVersion < this.minimumPolicyVersion) {
      return false;
    }

    // Otherwise the user was already notified.
    return true;
  },

  /**
   * Test only method, restarts the policy.
   */
  reset() {
    this.shutdown();
    this._isFirstRun = undefined;
    this._ensureUserIsNotifiedPromise = undefined;
    return this.setup();
  },

  /**
   * Setup the policy.
   */
  setup() {
    this._log.trace("setup");

    // Migrate the data choices infobar, if needed.
    this._migratePreferences();

    // Add the event observers.
    Services.obs.addObserver(this, "sessionstore-windows-restored");
  },

  /**
   * Clean up the reporting policy.
   */
  shutdown() {
    this._log.trace("shutdown");

    this._detachObservers();

    Policy.clearShowInfobarTimeout(this._startupNotificationTimerId);
  },

  /**
   * Detach the observers that were attached during setup.
   */
  _detachObservers() {
    Services.obs.removeObserver(this, "sessionstore-windows-restored");
  },

  /**
   * Check if we are allowed to upload data. In order to submit data both these conditions
   * should be true:
   * - The data submission preference should be true.
   * - The datachoices infobar should have been displayed.
   *
   * @return {Boolean} True if we are allowed to upload data, false otherwise.
   */
  canUpload() {
    // If data submission is disabled, there's no point in showing the infobar. Just
    // forbid to upload.
    if (!this.dataSubmissionEnabled) {
      return false;
    }

    // Submission is enabled. We enable upload if user is notified or we need to bypass
    // the policy.
    const bypassNotification = Services.prefs.getBoolPref(
      TelemetryUtils.Preferences.BypassNotification,
      false
    );
    return this.isUserNotifiedOfCurrentPolicy || bypassNotification;
  },

  isFirstRun() {
    if (this._isFirstRun === undefined) {
      this._isFirstRun = Services.prefs.getBoolPref(
        TelemetryUtils.Preferences.FirstRun,
        true
      );
    }
    return this._isFirstRun;
  },

  /**
   * Migrate the data policy preferences, if needed.
   */
  _migratePreferences() {
    // Current prefs are mostly the same than the old ones, except for some deprecated ones.
    for (let pref of DEPRECATED_FHR_PREFS) {
      Services.prefs.clearUserPref(pref);
    }
  },

  /**
   * Determine whether the user should be notified.
   */
  _shouldNotify() {
    if (!this.dataSubmissionEnabled) {
      this._log.trace(
        "_shouldNotify - Data submission disabled by the policy."
      );
      return false;
    }

    const bypassNotification = Services.prefs.getBoolPref(
      TelemetryUtils.Preferences.BypassNotification,
      false
    );
    if (this.isUserNotifiedOfCurrentPolicy || bypassNotification) {
      this._log.trace(
        "_shouldNotify - User already notified or bypassing the policy."
      );
      return false;
    }

    if (this._notificationInProgress) {
      this._log.trace(
        "_shouldNotify - User not notified, notification already in progress."
      );
      return false;
    }

    return true;
  },

  /**
   * Show the data choices infobar if needed.
   *
   * @param {function} resolve invoked with `true` if user was notified, `false`
   * if user was not notified.
   */
  _showInfobar(resolve) {
    if (!this._shouldNotify()) {
      this._log.trace("_showInfobar - User already notified, nothing to do.");
      resolve(false);
      return;
    }

    this._log.trace("_showInfobar - User not notified, notifying now.");
    this._notificationInProgress = true;
    let request = new NotifyPolicyRequest(this._log, resolve);
    Observers.notify("datareporting:notify-data-policy:request", request);
  },

  /**
   * Called when the user is notified with the infobar or otherwise.
   */
  _userNotified() {
    this._log.trace("_userNotified");
    this._recordNotificationData();
    lazy.TelemetrySend.notifyCanUpload();
  },

  /**
   * Record date and the version of the accepted policy.
   */
  _recordNotificationData() {
    this._log.trace("_recordNotificationData");
    this.dataSubmissionPolicyNotifiedDate = Policy.now();
    this.dataSubmissionPolicyAcceptedVersion = this.currentPolicyVersion;
    // The user was notified and the notification data saved: the notification
    // is no longer in progress.
    this._notificationInProgress = false;
  },

  /**
   * Try to open the privacy policy in a background tab instead of showing the infobar.
   *
   * @return {Promise<boolean>} Resolves to `true` if the user was notified via
   *                            background tab, `false` if we should fallback to
   *                            an infobar.
   */
  async _openFirstRunPage() {
    if (!this._shouldNotify()) {
      return false;
    }

    let firstRunPolicyURL = Services.prefs.getStringPref(
      TelemetryUtils.Preferences.FirstRunURL,
      ""
    );
    if (!firstRunPolicyURL) {
      return false;
    }
    firstRunPolicyURL = Services.urlFormatter.formatURL(firstRunPolicyURL);

    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    let win = BrowserWindowTracker.getTopWindow();

    if (!win) {
      this._log.info(
        "Couldn't find browser window to open first-run page. Falling back to infobar."
      );
      return false;
    }

    return new Promise(resolve => {
      // We'll consider the user notified once the privacy policy has been loaded
      // in a background tab even if that tab hasn't been selected.
      let tab;
      let progressListener = {};
      progressListener.onStateChange = (
        aBrowser,
        aWebProgress,
        aRequest,
        aStateFlags
      ) => {
        if (
          aWebProgress.isTopLevel &&
          tab &&
          tab.linkedBrowser == aBrowser &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK
        ) {
          removeListeners();

          let uri = aBrowser.documentURI;
          if (
            uri &&
            !/^about:(blank|neterror|certerror|blocked)/.test(uri.spec)
          ) {
            resolve(true);
          } else {
            this._log.info("Failed to load first-run page.");
            resolve(false);
          }
        }
      };

      let removeListeners = () => {
        win.removeEventListener("unload", removeListeners);
        win.gBrowser.removeTabsProgressListener(progressListener);
      };

      win.addEventListener("unload", removeListeners);
      win.gBrowser.addTabsProgressListener(progressListener);

      tab = win.gBrowser.addTab(firstRunPolicyURL, {
        inBackground: true,
        triggeringPrincipal:
          Services.scriptSecurityManager.getSystemPrincipal(),
      });
    });
  },

  observe(aSubject, aTopic) {
    if (aTopic == "sessionstore-windows-restored") {
      this._delayedSetup();
    }
  },

  /**
   * Handle initialization/configuration that happens at
   * `sessionstore-windows-restored` time.
   */
  async _delayedSetup() {
    // We're ready to make decisions about how to notify the user.  We only read
    // the Nimbus features once, so that Nimbus features changing doesn't yield
    // inconsistent results.  We also configure the datareporting policy Gecko
    // preferences based on the Nimbus `preonboarding` feature variables.  This
    // makes sense because we don't have support for re-notifying a user
    // _during_ the Firefox process lifetime; right now, we only notify the user
    // at Firefox startup.
    await this._configureFromNimbus();

    if (this.isFirstRun()) {
      // We're performing the first run, flip firstRun preference for subsequent runs.
      Services.prefs.setBoolPref(TelemetryUtils.Preferences.FirstRun, false);
    }

    if (!this._shouldNotify()) {
      this._log.trace(
        `observe: user has already been notified, no further action required`
      );
      return;
    }

    this.ensureUserIsNotified().then(() => {
      this._log.debug("_delayedSetup: marking user notified");
      this._userNotified();
    });
  },

  async _waitForUserIsNotified() {
    if (!this._shouldNotify()) {
      this._log.trace(
        `_waitForUserIsNotified: user has already been notified, no further action required`
      );
      return;
    }

    // We're about to show the user the modal dialog.
    // Make sure Glean won't initialize on shutdown, in case the user never interacts with the modal
    Services.prefs.setBoolPref("telemetry.fog.init_on_shutdown", false);

    if (this._nimbusVariables.enabled && this._nimbusVariables.screens) {
      if (await this._notifyUserViaMessagingSystem()) {
        this._log.trace(
          `_waitForUserIsNotified: user notified via Messaging System`
        );
        return;
      }
      `_waitForUserIsNotified: user not notified via Messaging System, falling back to legacy notification`;
    }

    await this._notifyUserViaTabOrInfobar();
  },

  /**
   * Notify user of privacy policy via background tab or (possibly after falling
   * back) via infobar.  The user is considered notified after the background
   * tab is loaded without error, or after the user has been shown (but not
   * necessarily interacted with) an infobar.
   *
   * @return {Promise<void>} Resolves after user is notified.
   */
  async _notifyUserViaTabOrInfobar() {
    if (this.isFirstRun()) {
      try {
        if (await this._openFirstRunPage()) {
          this._log.trace(
            `_notifyUserViaTabOrInfobar: user notified via browser tab`
          );
          return;
        }
      } catch (e) {
        this._log.error("Failed to open privacy policy tab: " + e);
      }
    }

    // Show the info bar.
    const delay = this.isFirstRun()
      ? NOTIFICATION_DELAY_FIRST_RUN_MSEC
      : NOTIFICATION_DELAY_NEXT_RUNS_MSEC;

    this._log.trace(
      `_notifyUserViaTabOrInfobar: notifying user via infobar after ${delay} milliseconds`
    );

    let p = new Promise(resolve => {
      this._startupNotificationTimerId = Policy.setShowInfobarTimeout(
        // Calling |canUpload| eventually shows the infobar, if needed.
        () => this._showInfobar(resolve),
        delay
      );
    });
    await p;
  },

  /**
   * If the preonboarding feature does not require interaction, resolve
   * immediately.  If the preonboarding feature does require interaction and the
   * required interaction has been completed, resolve immediately.  Otherwise,
   * wait until the required interaction is completed.
   *
   * @return Promise<void>
   * @throws {Error} when called before `sessionstore-windows-restored` notification.
   */
  async ensureUserIsNotified() {
    if (!this._ensureUserIsNotifiedPromise) {
      this._ensureUserIsNotifiedPromise = this._waitForUserIsNotified();
    }

    return this._ensureUserIsNotifiedPromise.then(() => {
      // The user has been notified and interacted with the modal.
      // Glean can now init on shutdown if necessary.
      Services.prefs.setBoolPref("telemetry.fog.init_on_shutdown", true);
    });
  },

  /**
   * Capture Nimbus configuration: record feature variables for future use and
   * set Gecko preferences based on values.
   */
  async _configureFromNimbus() {
    if (AppConstants.MOZ_BUILD_APP != "browser") {
      // OnboardingMessageProvider is browser/ only
      return;
    }
    this._nimbusVariables = lazy.NimbusFeatures.preonboarding.getAllVariables();

    if (this._nimbusVariables.enabled === null) {
      const PREF_TOS_ROLLOUT_ENROLLED =
        "browser.preonboarding.enrolledInOnTrainRollout";
      const preonboardingMessage =
        lazy.OnboardingMessageProvider.getPreonboardingMessages().find(
          m => m.id === "NEW_USER_TOU_ONBOARDING"
        );
      // Use default message variables, overriding with values from any set
      // fallback prefs.
      this._nimbusVariables = {
        ...preonboardingMessage,
        ...Object.fromEntries(
          Object.entries(this._nimbusVariables).filter(
            ([_, value]) => value !== null
          )
        ),
      };
      Services.prefs.setBoolPref(PREF_TOS_ROLLOUT_ENROLLED, true);
      this._log.trace(
        `_configureFromNimbus: using default preonboarding message`
      );
    }

    if (this._nimbusVariables.enabled) {
      if ("currentPolicyVersion" in this._nimbusVariables) {
        this._log.trace(
          `_configureFromNimbus: setting currentPolicyVersion from Nimbus feature (${this._nimbusVariables.currentPolicyVersion})`
        );
        Services.prefs.setIntPref(
          TelemetryUtils.Preferences.CurrentPolicyVersion,
          this._nimbusVariables.currentPolicyVersion
        );
      }
      if ("minimumPolicyVersion" in this._nimbusVariables) {
        this._log.trace(
          `_configureFromNimbus: setting minimumPolicyVersion from Nimbus feature (${this._nimbusVariables.minimumPolicyVersion})`
        );
        Services.prefs.setIntPref(
          TelemetryUtils.Preferences.MinimumPolicyVersion,
          this._nimbusVariables.minimumPolicyVersion
        );
      }
      if ("firstRunURL" in this._nimbusVariables) {
        this._log.trace(
          `_configureFromNimbus: setting firstRunURL from Nimbus feature ('${this._nimbusVariables.firstRunURL}')`
        );
        Services.prefs.setCharPref(
          TelemetryUtils.Preferences.FirstRunURL,
          this._nimbusVariables.firstRunURL
        );
      }
    } else {
      this._log.trace(
        `_configureFromNimbus: Nimbus feature disabled, not setting preferences`
      );
    }
  },

  async _showModal(data) {
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    );
    const { SpecialMessageActions } = ChromeUtils.importESModule(
      "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
    );

    let win = BrowserWindowTracker.getTopWindow();

    const config = {
      type: "SHOW_SPOTLIGHT",
      data: {
        content: {
          template: "multistage",
          id: data?.id || "PRE_ONBOARDING_MODAL",
          backdrop: data?.backdrop,
          screens: data.screens,
          UTMTerm: data?.UTMTerm,
          disableEscClose: data?.requireAction,
          // displayed as a window modal by default
        },
      },
    };

    SpecialMessageActions.handleAction(config, win);
    this._notificationInProgress = true;

    return true;
  },

  /**
   * Notify the user via the Firefox Messaging System (e.g., a modal dialog) and
   * wait for user interaction.
   *
   * User interaction is signaled by the
   * `datareporting:notify-data-policy:interacted` observer notification.
   *
   * @return {Promise<boolean>} `true` if user was notified, `false` to fallback
   * to legacy tab/infobar notification.
   */
  async _notifyUserViaMessagingSystem() {
    let p = lazy.BrowserUtils.promiseObserved(
      "datareporting:notify-data-policy:interacted"
    );

    if (!(await Policy.showModal(this._nimbusVariables))) {
      this._log.trace(
        "_notifyUserViaModal: notification was not displayed, falling back to legacy notification"
      );
      return false;
    }

    this._log.trace(
      "_notifyUserViaModal: modal displayed, waiting for user interaction"
    );

    // On Windows, clear the jump list to limit opening new windows while the
    // modal is displayed
    if (AppConstants.platform === "win") {
      lazy.WinTaskbarJumpList.blockJumpList(p);
    }

    await p;
    this._log.trace("_notifyUserViaModal: user interacted with modal");

    return true;
  },
};
