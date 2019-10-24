/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.importGlobalProperties(["fetch"]);

var EXPORTED_SYMBOLS = ["AboutProtectionsHandler"];
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { RemotePages } = ChromeUtils.import(
  "resource://gre/modules/remotepagemanager/RemotePageManagerParent.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  fxAccounts: "resource://gre/modules/FxAccounts.jsm",
  FXA_PWDMGR_HOST: "resource://gre/modules/FxAccountsCommon.js",
  FXA_PWDMGR_REALM: "resource://gre/modules/FxAccountsCommon.js",
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  LoginBreaches: "resource:///modules/LoginBreaches.jsm",
  LoginHelper: "resource://gre/modules/LoginHelper.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "TrackingDBService",
  "@mozilla.org/tracking-db-service;1",
  "nsITrackingDBService"
);

let idToTextMap = new Map([
  [Ci.nsITrackingDBService.TRACKERS_ID, "tracker"],
  [Ci.nsITrackingDBService.TRACKING_COOKIES_ID, "cookie"],
  [Ci.nsITrackingDBService.CRYPTOMINERS_ID, "cryptominer"],
  [Ci.nsITrackingDBService.FINGERPRINTERS_ID, "fingerprinter"],
  [Ci.nsITrackingDBService.SOCIAL_ID, "social"],
]);

const MONITOR_API_ENDPOINT = "https://monitor.firefox.com/user/breach-stats";

const SECURE_PROXY_ADDON_ID = "secure-proxy@mozilla.com";

// TODO: there will be a monitor-specific scope for FxA access tokens, which we should be
// using once it's implemented. See: https://github.com/mozilla/blurts-server/issues/1128
const SCOPE_MONITOR = [
  "profile:uid",
  "https://identity.mozilla.com/apps/monitor",
];

// Error messages
const INVALID_OAUTH_TOKEN = "Invalid OAuth token";
const USER_UNSUBSCRIBED_TO_MONITOR = "User is not subscribed to Monitor";
const SERVICE_UNAVAILABLE = "Service unavailable";
const UNEXPECTED_RESPONSE = "Unexpected response";
const UNKNOWN_ERROR = "Unknown error";

// Valid response info for successful Monitor data
const MONITOR_RESPONSE_PROPS = ["monitoredEmails", "numBreaches", "passwords"];

var AboutProtectionsHandler = {
  _inited: false,
  monitorResponse: null,
  _topics: [
    "ClearMonitorCache",
    // Opening about:* pages
    "OpenAboutLogins",
    "OpenContentBlockingPreferences",
    "OpenSyncPreferences",
    // Fetching data
    "FetchContentBlockingEvents",
    "FetchMonitorData",
    "FetchUserLoginsData",
    "GetShowProxyCard",
  ],

  init() {
    this.receiveMessage = this.receiveMessage.bind(this);
    this.pageListener = new RemotePages("about:protections");
    for (let topic of this._topics) {
      this.pageListener.addMessageListener(topic, this.receiveMessage);
    }
    Services.telemetry.setEventRecordingEnabled(
      "security.ui.protections",
      true
    );
    this._inited = true;
  },

  uninit() {
    if (!this._inited) {
      return;
    }
    for (let topic of this._topics) {
      this.pageListener.removeMessageListener(topic, this.receiveMessage);
    }
    this.pageListener.destroy();
  },

  /**
   * Fetches and validates data from the Monitor endpoint. If successful, then return
   * expected data. Otherwise, throw the appropriate error depending on the status code.
   *
   * @return valid data from endpoint.
   */
  async fetchUserBreachStats(token) {
    if (this.monitorResponse && this.monitorResponse.timestamp) {
      var timeDiff = Date.now() - this.monitorResponse.timestamp;
      let oneDayInMS = 24 * 60 * 60 * 1000;
      if (timeDiff >= oneDayInMS) {
        this.monitorResponse = null;
      } else {
        return this.monitorResponse;
      }
    }

    // Make the request
    const headers = new Headers();
    headers.append("Authorization", `Bearer ${token}`);
    const request = new Request(MONITOR_API_ENDPOINT, { headers });
    const response = await fetch(request);

    if (response.ok) {
      // Validate the shape of the response is what we're expecting.
      const json = await response.json();

      // Make sure that we're getting the expected data.
      let isValid = null;
      for (let prop in json) {
        isValid = MONITOR_RESPONSE_PROPS.includes(prop);

        if (!isValid) {
          break;
        }
      }

      this.monitorResponse = isValid ? json : new Error(UNEXPECTED_RESPONSE);
      if (isValid) {
        this.monitorResponse.timestamp = Date.now();
      }
    } else {
      // Check the reason for the error
      switch (response.status) {
        case 400:
        case 401:
          this.monitorResponse = new Error(INVALID_OAUTH_TOKEN);
          break;
        case 404:
          this.monitorResponse = new Error(USER_UNSUBSCRIBED_TO_MONITOR);
          break;
        case 503:
          this.monitorResponse = new Error(SERVICE_UNAVAILABLE);
          break;
        default:
          this.monitorResponse = new Error(UNKNOWN_ERROR);
          break;
      }
    }

    if (this.monitorResponse instanceof Error) {
      throw this.monitorResponse;
    }
    return this.monitorResponse;
  },

  /**
   * Retrieves login data for the user.
   *
   * @return {{ hasFxa: Boolean,
   *            numLogins: Number,
   *            numSyncedDevices: Number }}
   *         The login data.
   */
  async getLoginData() {
    let hasFxa = false;

    try {
      if ((hasFxa = !!(await fxAccounts.getSignedInUser()))) {
        await fxAccounts.device.refreshDeviceList();
      }
    } catch (e) {
      Cu.reportError("There was an error fetching login data: ", e.message);
    }

    const userFacingLogins =
      Services.logins.countLogins("", "", "") -
      Services.logins.countLogins(FXA_PWDMGR_HOST, null, FXA_PWDMGR_REALM);

    return {
      hasFxa,
      numLogins: userFacingLogins,
      numSyncedDevices: fxAccounts.device.recentDeviceList
        ? fxAccounts.device.recentDeviceList.length
        : 0,
    };
  },

  /**
   * Retrieves monitor data for the user.
   *
   * @return {{ monitoredEmails: Number,
   *            numBreaches: Number,
   *            passwords: Number,
   *            userEmail: String|null,
   *            potentiallyBreachedLogins: Number,
   *            error: Boolean }}
   *         Monitor data.
   */
  async getMonitorData() {
    let monitorData = {};
    let potentiallyBreachedLogins = null;
    let userEmail = null;
    let token = await this.getMonitorScopedOAuthToken();

    try {
      if (token) {
        monitorData = await this.fetchUserBreachStats(token);

        // Get the stats for number of potentially breached Lockwise passwords if no master
        // password is set.
        if (!LoginHelper.isMasterPasswordSet()) {
          const logins = await LoginHelper.getAllUserFacingLogins();
          potentiallyBreachedLogins = await LoginBreaches.getPotentialBreachesByLoginGUID(
            logins
          );
        }
        // Send back user's email so the protections report can direct them to the proper
        // OAuth flow on Monitor.
        const { email } = await fxAccounts.getSignedInUser();
        userEmail = email;
      } else {
        // If no account exists, then the user is not logged in with an fxAccount.
        monitorData = {
          errorMessage: "No account",
        };
      }
    } catch (e) {
      Cu.reportError(e.message);
      monitorData.errorMessage = e.message;

      // If the user's OAuth token is invalid, we clear the cached token and refetch
      // again. If OAuth token is invalid after the second fetch, then the monitor UI
      // will simply show the "no logins" UI version.
      if (e.message === INVALID_OAUTH_TOKEN) {
        await fxAccounts.removeCachedOAuthToken({ token });
        token = await this.getMonitorScopedOAuthToken();

        try {
          monitorData = await this.fetchUserBreachStats(token);
        } catch (_) {
          Cu.reportError(e.message);
        }
      } else if (e.message === USER_UNSUBSCRIBED_TO_MONITOR) {
        // Send back user's email so the protections report can direct them to the proper
        // OAuth flow on Monitor.
        const { email } = await fxAccounts.getSignedInUser();
        userEmail = email;
      } else {
        monitorData.errorMessage = e.message || "An error ocurred.";
      }
    }

    return {
      ...monitorData,
      userEmail,
      potentiallyBreachedLogins: potentiallyBreachedLogins
        ? potentiallyBreachedLogins.size
        : 0,
      error: !!monitorData.errorMessage,
    };
  },

  async getMonitorScopedOAuthToken() {
    let token = null;

    try {
      token = await fxAccounts.getOAuthToken({ scope: SCOPE_MONITOR });
    } catch (e) {
      Cu.reportError(
        "There was an error fetching the user's token: ",
        e.message
      );
    }

    return token;
  },

  /**
   * The proxy card will only show if the user is in the US, has the browser language in "en-US",
   * and does not yet have Proxy installed.
   */
  async shouldShowProxyCard() {
    const region = Services.prefs.getCharPref("browser.search.region");
    const languages = Services.prefs.getComplexValue(
      "intl.accept_languages",
      Ci.nsIPrefLocalizedString
    );
    const alreadyInstalled = await AddonManager.getAddonByID(
      SECURE_PROXY_ADDON_ID
    );

    return (
      region.toLowerCase() === "us" &&
      !alreadyInstalled &&
      languages.data.toLowerCase().includes("en-us")
    );
  },

  /**
   * Sends a response from message target.
   *
   * @param {Object}  target
   *        The message target.
   * @param {String}  message
   *        The topic of the message to send.
   * @param {Object}  payload
   *        The payload of the message to send.
   */
  sendMessage(target, message, payload) {
    // Make sure the target's browser is available before sending.
    if (target.browser) {
      target.sendAsyncMessage(message, payload);
    }
  },

  async receiveMessage(aMessage) {
    let win = aMessage.target.browser.ownerGlobal;
    switch (aMessage.name) {
      case "OpenAboutLogins":
        LoginHelper.openPasswordManager(win, {
          entryPoint: "aboutprotections",
        });
        break;
      case "OpenContentBlockingPreferences":
        win.openPreferences("privacy-trackingprotection", {
          origin: "about-protections",
        });
        break;
      case "OpenSyncPreferences":
        win.openTrustedLinkIn("about:preferences#sync", "tab");
        break;
      case "FetchContentBlockingEvents":
        let sumEvents = await TrackingDBService.sumAllEvents();
        let earliestDate = await TrackingDBService.getEarliestRecordedDate();
        let eventsByDate = await TrackingDBService.getEventsByDateRange(
          aMessage.data.from,
          aMessage.data.to
        );
        let dataToSend = {};
        let largest = 0;

        for (let result of eventsByDate) {
          let count = result.getResultByName("count");
          let type = result.getResultByName("type");
          let timestamp = result.getResultByName("timestamp");
          dataToSend[timestamp] = dataToSend[timestamp] || { total: 0 };
          dataToSend[timestamp][idToTextMap.get(type)] = count;
          dataToSend[timestamp].total += count;
          // Record the largest amount of tracking events found per day,
          // to create the tallest column on the graph and compare other days to.
          if (largest < dataToSend[timestamp].total) {
            largest = dataToSend[timestamp].total;
          }
        }
        dataToSend.largest = largest;
        dataToSend.earliestDate = earliestDate;
        dataToSend.sumEvents = sumEvents;

        let weekdays = Services.intl.getDisplayNames(undefined, {
          style: "short",
          keys: [
            "dates/gregorian/weekdays/sunday",
            "dates/gregorian/weekdays/monday",
            "dates/gregorian/weekdays/tuesday",
            "dates/gregorian/weekdays/wednesday",
            "dates/gregorian/weekdays/thursday",
            "dates/gregorian/weekdays/friday",
            "dates/gregorian/weekdays/saturday",
            "dates/gregorian/weekdays/sunday",
          ],
        });
        weekdays = Object.values(weekdays.values);
        dataToSend.weekdays = weekdays;

        this.sendMessage(
          aMessage.target,
          "SendContentBlockingRecords",
          dataToSend
        );
        break;
      case "FetchMonitorData":
        this.sendMessage(
          aMessage.target,
          "SendMonitorData",
          await this.getMonitorData()
        );
        break;
      case "FetchUserLoginsData":
        this.sendMessage(
          aMessage.target,
          "SendUserLoginsData",
          await this.getLoginData()
        );
        break;
      case "ClearMonitorCache":
        this.monitorResponse = null;
        break;
      case "GetShowProxyCard":
        if (await this.shouldShowProxyCard()) {
          this.sendMessage(aMessage.target, "SendShowProxyCard");
        }
    }
  },
};
