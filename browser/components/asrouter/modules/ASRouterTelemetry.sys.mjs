/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We use importESModule here instead of static import so that the Karma test
// environment won't choke on these module. This is because the Karma test
// environment already stubs out XPCOMUtils, AppConstants and RemoteSettings,
// and overrides importESModule to be a no-op (which can't be done for a static
// import statement).

// eslint-disable-next-line mozilla/use-static-import
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

// eslint-disable-next-line mozilla/use-static-import
const { MESSAGE_TYPE_HASH: msg } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ActorConstants.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ClientID: "resource://gre/modules/ClientID.sys.mjs",
  AboutWelcomeTelemetry:
    "resource:///modules/aboutwelcome/AboutWelcomeTelemetry.sys.mjs",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  TelemetrySession: "resource://gre/modules/TelemetrySession.sys.mjs",
  UpdateUtils: "resource://gre/modules/UpdateUtils.sys.mjs",
});
ChromeUtils.defineLazyGetter(
  lazy,
  "Telemetry",
  () => new lazy.AboutWelcomeTelemetry()
);

ChromeUtils.defineLazyGetter(
  lazy,
  "browserSessionId",
  () => lazy.TelemetrySession.getMetadata("").sessionId
);

export const PREF_IMPRESSION_ID =
  "browser.newtabpage.activity-stream.impressionId";

export class ASRouterTelemetry {
  constructor() {
    this._impressionId = this.getOrCreateImpressionId();
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "telemetryEnabled",
      "browser.newtabpage.activity-stream.telemetry",
      false
    );
  }

  get telemetryClientId() {
    Object.defineProperty(this, "telemetryClientId", {
      value: lazy.ClientID.getClientID(),
    });
    return this.telemetryClientId;
  }

  getOrCreateImpressionId() {
    let impressionId = Services.prefs.getCharPref(PREF_IMPRESSION_ID, "");
    if (!impressionId) {
      impressionId = String(Services.uuid.generateUUID());
      Services.prefs.setCharPref(PREF_IMPRESSION_ID, impressionId);
    }
    return impressionId;
  }
  /**
   *  Check if it is in the CFR experiment cohort by querying against the
   *  experiment manager of Messaging System
   *
   *  @return {bool}
   */
  get isInCFRCohort() {
    const experimentData = lazy.ExperimentAPI.getExperimentMetaData({
      featureId: "cfr",
    });
    if (experimentData && experimentData.slug) {
      return true;
    }

    return false;
  }

  /**
   * Create a ping for AS router event. The client_id is set to "n/a" by default,
   * different component can override this by its own telemetry collection policy.
   */
  async createASRouterEvent(action) {
    let event = {
      ...action.data,
      addon_version: Services.appinfo.appBuildID,
      locale: Services.locale.appLocaleAsBCP47,
    };

    if (event.event_context && typeof event.event_context === "object") {
      event.event_context = JSON.stringify(event.event_context);
    }
    switch (event.action) {
      case "cfr_user_event":
        event = await this.applyCFRPolicy(event);
        break;
      case "badge_user_event":
        event = await this.applyToolbarBadgePolicy(event);
        break;
      case "infobar_user_event":
        event = await this.applyInfoBarPolicy(event);
        break;
      case "spotlight_user_event":
        event = await this.applySpotlightPolicy(event);
        break;
      case "toast_notification_user_event":
        event = await this.applyToastNotificationPolicy(event);
        break;
      case "moments_user_event":
        event = await this.applyMomentsPolicy(event);
        break;
      case "menu_message_user_event":
        event = await this.applyMenuMessagePolicy(event);
        break;
      case "asrouter_undesired_event":
        event = this.applyUndesiredEventPolicy(event);
        break;
      default:
        event = { ping: event };
        break;
    }
    return event;
  }

  /**
   * Per Bug 1484035, CFR metrics comply with following policies:
   * 1). In release, it collects impression_id and bucket_id
   * 2). In prerelease, it collects client_id and message_id
   * 3). In shield experiments conducted in release, it collects client_id and message_id
   * 4). In Private Browsing windows, unless in experiment, collects impression_id and bucket_id
   */
  async applyCFRPolicy(ping) {
    if (
      (lazy.UpdateUtils.getUpdateChannel(true) === "release" ||
        ping.is_private) &&
      !this.isInCFRCohort
    ) {
      ping.message_id = "n/a";
      ping.impression_id = this._impressionId;
    } else {
      ping.client_id = await this.telemetryClientId;
    }
    delete ping.action;
    delete ping.is_private;
    return { ping, pingType: "cfr" };
  }

  /**
   * Per Bug 1482134, all the metrics for What's New panel use client_id in
   * all the release channels
   */
  async applyToolbarBadgePolicy(ping) {
    ping.client_id = await this.telemetryClientId;
    ping.browser_session_id = lazy.browserSessionId;
    // Attach page info to `event_context` if there is a session associated with this ping
    delete ping.action;
    return { ping, pingType: "toolbar-badge" };
  }

  async applyInfoBarPolicy(ping) {
    ping.client_id = await this.telemetryClientId;
    ping.browser_session_id = lazy.browserSessionId;
    delete ping.action;
    return { ping, pingType: "infobar" };
  }

  async applySpotlightPolicy(ping) {
    ping.client_id = await this.telemetryClientId;
    ping.browser_session_id = lazy.browserSessionId;
    delete ping.action;
    return { ping, pingType: "spotlight" };
  }

  async applyToastNotificationPolicy(ping) {
    ping.client_id = await this.telemetryClientId;
    ping.browser_session_id = lazy.browserSessionId;
    delete ping.action;
    return { ping, pingType: "toast_notification" };
  }

  async applyMenuMessagePolicy(ping) {
    ping.client_id = await this.telemetryClientId;
    ping.browser_session_id = lazy.browserSessionId;
    delete ping.action;
    return { ping, pingType: "menu" };
  }

  /**
   * Per Bug 1484035, Moments metrics comply with following policies:
   * 1). In release, it collects impression_id, and treats bucket_id as message_id
   * 2). In prerelease, it collects client_id and message_id
   * 3). In shield experiments conducted in release, it collects client_id and message_id
   */
  async applyMomentsPolicy(ping) {
    if (
      lazy.UpdateUtils.getUpdateChannel(true) === "release" &&
      !this.isInCFRCohort
    ) {
      ping.message_id = "n/a";
      ping.impression_id = this._impressionId;
    } else {
      ping.client_id = await this.telemetryClientId;
    }
    delete ping.action;
    return { ping, pingType: "moments" };
  }

  applyUndesiredEventPolicy(ping) {
    ping.impression_id = this._impressionId;
    delete ping.action;
    return { ping, pingType: "undesired-events" };
  }

  async handleASRouterUserEvent(action) {
    const { ping, pingType } = await this.createASRouterEvent(action);
    if (!pingType) {
      console.error("Unknown ping type for ASRouter telemetry");
      return;
    }

    // Now that the action has become a ping, we can echo it to Glean.
    if (this.telemetryEnabled) {
      lazy.Telemetry.submitGleanPingForPing({ ...ping, pingType });
    }
  }

  /**
   * This function is used by ActivityStreamStorage to report errors
   * trying to access IndexedDB.
   */
  SendASRouterUndesiredEvent(data) {
    this.handleASRouterUserEvent({
      data: { ...data, action: "asrouter_undesired_event" },
    });
  }

  onAction(action) {
    switch (action.type) {
      // The remaining action types come from ASRouter, which doesn't use
      // Actions from Actions.mjs, but uses these other custom strings.
      case msg.TOOLBAR_BADGE_TELEMETRY:
      // Intentional fall-through
      case msg.TOOLBAR_PANEL_TELEMETRY:
      // Intentional fall-through
      case msg.MOMENTS_PAGE_TELEMETRY:
      // Intentional fall-through
      case msg.DOORHANGER_TELEMETRY:
      // Intentional fall-through
      case msg.INFOBAR_TELEMETRY:
      // Intentional fall-through
      case msg.SPOTLIGHT_TELEMETRY:
      // Intentional fall-through
      case msg.TOAST_NOTIFICATION_TELEMETRY:
      // Intentional fall-through
      case msg.MENU_MESSAGE_TELEMETRY:
      // Intentional fall-through
      case msg.AS_ROUTER_TELEMETRY_USER_EVENT:
        this.handleASRouterUserEvent(action);
        break;
    }
  }
}
