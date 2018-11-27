/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const promise = require("promise");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");

loader.lazyRequireGetter(this, "DebuggerClient", "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "throttlingProfiles", "devtools/client/shared/components/throttling/profiles");
loader.lazyRequireGetter(this, "SettingOnboardingTooltip", "devtools/client/responsive.html/setting-onboarding-tooltip");
loader.lazyRequireGetter(this, "swapToInnerBrowser", "devtools/client/responsive.html/browser/swap", true);
loader.lazyRequireGetter(this, "startup", "devtools/client/responsive.html/utils/window", true);
loader.lazyRequireGetter(this, "message", "devtools/client/responsive.html/utils/message");
loader.lazyRequireGetter(this, "showNotification", "devtools/client/responsive.html/utils/notification", true);
loader.lazyRequireGetter(this, "l10n", "devtools/client/responsive.html/utils/l10n");
loader.lazyRequireGetter(this, "EmulationFront", "devtools/shared/fronts/emulation", true);
loader.lazyRequireGetter(this, "PriorityLevels", "devtools/client/shared/components/NotificationBox", true);
loader.lazyRequireGetter(this, "TargetFactory", "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "Telemetry", "devtools/client/shared/telemetry");
loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");

const TOOL_URL = "chrome://devtools/content/responsive.html/index.xhtml";

const RELOAD_CONDITION_PREF_PREFIX = "devtools.responsive.reloadConditions.";
const RELOAD_NOTIFICATION_PREF = "devtools.responsive.reloadNotification.enabled";
const SHOW_SETTING_TOOLTIP_PREF = "devtools.responsive.show-setting-tooltip";

function debug(msg) {
  // console.log(`RDM manager: ${msg}`);
}

/**
 * ResponsiveUIManager is the external API for the browser UI, etc. to use when
 * opening and closing the responsive UI.
 */
const ResponsiveUIManager = exports.ResponsiveUIManager = {
  _telemetry: new Telemetry(),

  activeTabs: new Map(),

  /**
   * Toggle the responsive UI for a tab.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   * @param options
   *        Other options associated with toggling.  Currently includes:
   *        - `trigger`: String denoting the UI entry point, such as:
   *          - `toolbox`:  Toolbox Button
   *          - `menu`:     Web Developer menu item
   *          - `shortcut`: Keyboard shortcut
   * @return Promise
   *         Resolved when the toggling has completed.  If the UI has opened,
   *         it is resolved to the ResponsiveUI instance for this tab.  If the
   *         the UI has closed, there is no resolution value.
   */
  toggle(window, tab, options = {}) {
    const action = this.isActiveForTab(tab) ? "close" : "open";
    const completed = this[action + "IfNeeded"](window, tab, options);
    completed.catch(console.error);
    return completed;
  },

  /**
   * Opens the responsive UI, if not already open.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   * @param options
   *        Other options associated with opening.  Currently includes:
   *        - `trigger`: String denoting the UI entry point, such as:
   *          - `toolbox`:  Toolbox Button
   *          - `menu`:     Web Developer menu item
   *          - `shortcut`: Keyboard shortcut
   * @return Promise
   *         Resolved to the ResponsiveUI instance for this tab when opening is
   *         complete.
   */
  async openIfNeeded(window, tab, options = {}) {
    if (!tab.linkedBrowser.isRemoteBrowser) {
      await this.showRemoteOnlyNotification(window, tab, options);
      return promise.reject(new Error("RDM only available for remote tabs."));
    }
    if (!this.isActiveForTab(tab)) {
      this.initMenuCheckListenerFor(window);

      const ui = new ResponsiveUI(window, tab);
      this.activeTabs.set(tab, ui);

      // Explicitly not await on telemetry to avoid delaying RDM opening
      this.recordTelemetryOpen(window, tab, options);

      await this.setMenuCheckFor(tab, window);
      await ui.inited;
      this.emit("on", { tab });
    }

    return this.getResponsiveUIForTab(tab);
  },

  /**
   * Record all telemetry probes related to RDM opening.
   */
  async recordTelemetryOpen(window, tab, options) {
    // Track whether a toolbox was opened before RDM was opened.
    const isKnownTab = TargetFactory.isKnownTab(tab);
    let toolbox;
    if (isKnownTab) {
      const target = await TargetFactory.forTab(tab);
      toolbox = gDevTools.getToolbox(target);
    }
    const hostType = toolbox ? toolbox.hostType : "none";
    const hasToolbox = !!toolbox;
    const tel = this._telemetry;
    if (hasToolbox) {
      tel.scalarAdd("devtools.responsive.toolbox_opened_first", 1);
    }

    tel.recordEvent("activate", "responsive_design", null, {
      "host": hostType,
      "width": Math.ceil(window.outerWidth / 50) * 50,
      "session_id": toolbox ? toolbox.sessionId : -1,
    });

    // Track opens keyed by the UI entry point used.
    let { trigger } = options;
    if (!trigger) {
      trigger = "unknown";
    }
    tel.keyedScalarAdd("devtools.responsive.open_trigger", trigger, 1);
  },

  /**
   * Closes the responsive UI, if not already closed.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   * @param options
   *        Other options associated with closing.  Currently includes:
   *        - `trigger`: String denoting the UI entry point, such as:
   *          - `toolbox`:  Toolbox Button
   *          - `menu`:     Web Developer menu item
   *          - `shortcut`: Keyboard shortcut
   *        - `reason`: String detailing the specific cause for closing
   * @return Promise
   *         Resolved (with no value) when closing is complete.
   */
  async closeIfNeeded(window, tab, options = {}) {
    if (this.isActiveForTab(tab)) {
      const ui = this.activeTabs.get(tab);
      const destroyed = await ui.destroy(options);
      if (!destroyed) {
        // Already in the process of destroying, abort.
        return;
      }

      this.activeTabs.delete(tab);

      if (!this.isActiveForWindow(window)) {
        this.removeMenuCheckListenerFor(window);
      }
      this.emit("off", { tab });
      await this.setMenuCheckFor(tab, window);

      // Explicitly not await on telemetry to avoid delaying RDM closing
      this.recordTelemetryClose(window, tab);
    }
  },

  async recordTelemetryClose(window, tab) {
    const isKnownTab = TargetFactory.isKnownTab(tab);
    let toolbox;
    if (isKnownTab) {
      const target = await TargetFactory.forTab(tab);
      toolbox = gDevTools.getToolbox(target);
    }

    const hostType = toolbox ? toolbox.hostType : "none";
    const t = this._telemetry;
    t.recordEvent("deactivate", "responsive_design", null, {
      "host": hostType,
      "width": Math.ceil(window.outerWidth / 50) * 50,
      "session_id": toolbox ? toolbox.sessionId : -1,
    });
  },

  /**
   * Returns true if responsive UI is active for a given tab.
   *
   * @param tab
   *        The browser tab.
   * @return boolean
   */
  isActiveForTab(tab) {
    return this.activeTabs.has(tab);
  },

  /**
   * Returns true if responsive UI is active in any tab in the given window.
   *
   * @param window
   *        The main browser chrome window.
   * @return boolean
   */
  isActiveForWindow(window) {
    return [...this.activeTabs.keys()].some(t => t.ownerGlobal === window);
  },

  /**
   * Return the responsive UI controller for a tab.
   *
   * @param tab
   *        The browser tab.
   * @return ResponsiveUI
   *         The UI instance for this tab.
   */
  getResponsiveUIForTab(tab) {
    return this.activeTabs.get(tab);
  },

  handleMenuCheck({target}) {
    ResponsiveUIManager.setMenuCheckFor(target);
  },

  initMenuCheckListenerFor(window) {
    const { tabContainer } = window.gBrowser;
    tabContainer.addEventListener("TabSelect", this.handleMenuCheck);
  },

  removeMenuCheckListenerFor(window) {
    if (window && window.gBrowser && window.gBrowser.tabContainer) {
      const { tabContainer } = window.gBrowser;
      tabContainer.removeEventListener("TabSelect", this.handleMenuCheck);
    }
  },

  async setMenuCheckFor(tab, window = tab.ownerGlobal) {
    await startup(window);

    const menu = window.document.getElementById("menu_responsiveUI");
    if (menu) {
      menu.setAttribute("checked", this.isActiveForTab(tab));
    }
  },

  showRemoteOnlyNotification(window, tab, { trigger } = {}) {
    return showNotification(window, tab, {
      toolboxButton: trigger == "toolbox",
      msg: l10n.getStr("responsive.remoteOnly"),
      priority: PriorityLevels.PRIORITY_CRITICAL_MEDIUM,
    });
  },
};

EventEmitter.decorate(ResponsiveUIManager);

/**
 * ResponsiveUI manages the responsive design tool for a specific tab.  The
 * actual tool itself lives in a separate chrome:// document that is loaded into
 * the tab upon opening responsive design.  This object acts a helper to
 * integrate the tool into the surrounding browser UI as needed.
 */
function ResponsiveUI(window, tab) {
  this.browserWindow = window;
  this.tab = tab;
  this.inited = this.init();
}

ResponsiveUI.prototype = {

  /**
   * The main browser chrome window (that holds many tabs).
   */
  browserWindow: null,

  /**
   * The specific browser tab this responsive instance is for.
   */
  tab: null,

  /**
   * Promise resovled when the UI init has completed.
   */
  inited: null,

  /**
   * Flag set when destruction has begun.
   */
  destroying: false,

  /**
   * Flag set when destruction has ended.
   */
  destroyed: false,

  /**
   * A window reference for the chrome:// document that displays the responsive
   * design tool.  It is safe to reference this window directly even with e10s,
   * as the tool UI is always loaded in the parent process.  The web content
   * contained *within* the tool UI on the other hand is loaded in the child
   * process.
   */
  toolWindow: null,

  /**
   * Open RDM while preserving the state of the page.  We use `swapFrameLoaders`
   * to ensure all in-page state is preserved, just like when you move a tab to
   * a new window.
   *
   * For more details, see /devtools/docs/responsive-design-mode.md.
   */
  async init() {
    debug("Init start");

    const ui = this;

    // Watch for tab close and window close so we can clean up RDM synchronously
    this.tab.addEventListener("TabClose", this);
    this.browserWindow.addEventListener("unload", this);

    // Swap page content from the current tab into a viewport within RDM
    debug("Create browser swapper");
    this.swap = swapToInnerBrowser({
      tab: this.tab,
      containerURL: TOOL_URL,
      async getInnerBrowser(containerBrowser) {
        const toolWindow = ui.toolWindow = containerBrowser.contentWindow;
        toolWindow.addEventListener("message", ui);
        debug("Wait until init from inner");
        await message.request(toolWindow, "init");
        toolWindow.addInitialViewport({
          uri: "about:blank",
          userContextId: ui.tab.userContextId,
        });
        debug("Wait until browser mounted");
        await message.wait(toolWindow, "browser-mounted");
        return ui.getViewportBrowser();
      },
    });
    debug("Wait until swap start");
    await this.swap.start();

    this.tab.addEventListener("BeforeTabRemotenessChange", this);

    // Notify the inner browser to start the frame script
    debug("Wait until start frame script");
    await message.request(this.toolWindow, "start-frame-script");

    // Get the protocol ready to speak with emulation actor
    debug("Wait until RDP server connect");
    await this.connectToServer();

    // Restore the previous state of RDM.
    await this.restoreState();

    // Show the settings onboarding tooltip
    if (Services.prefs.getBoolPref(SHOW_SETTING_TOOLTIP_PREF)) {
      this.settingOnboardingTooltip =
        new SettingOnboardingTooltip(ui.toolWindow.document);
    }

    // Non-blocking message to tool UI to start any delayed init activities
    message.post(this.toolWindow, "post-init");

    debug("Init done");
  },

  /**
   * Close RDM and restore page content back into a regular tab.
   *
   * @param object
   *        Destroy options, which currently includes a `reason` string.
   * @return boolean
   *         Whether this call is actually destroying.  False means destruction
   *         was already in progress.
   */
  async destroy(options) {
    if (this.destroying) {
      return false;
    }
    this.destroying = true;

    // If our tab is about to be closed, there's not enough time to exit
    // gracefully, but that shouldn't be a problem since the tab will go away.
    // So, skip any waiting when we're about to close the tab.
    const isTabDestroyed = !this.tab.linkedBrowser;
    const isWindowClosing = (options && options.reason === "unload") || isTabDestroyed;
    const isTabContentDestroying =
      isWindowClosing || (options && (options.reason === "TabClose" ||
                                      options.reason === "BeforeTabRemotenessChange"));

    // Ensure init has finished before starting destroy
    if (!isTabContentDestroying) {
      await this.inited;
    }

    this.tab.removeEventListener("TabClose", this);
    this.tab.removeEventListener("BeforeTabRemotenessChange", this);
    this.browserWindow.removeEventListener("unload", this);
    this.toolWindow.removeEventListener("message", this);

    if (!isTabContentDestroying) {
      // Notify the inner browser to stop the frame script
      await message.request(this.toolWindow, "stop-frame-script");
    }

    // Ensure the tab is reloaded if required when exiting RDM so that no emulated
    // settings are left in a customized state.
    if (!isTabContentDestroying) {
      let reloadNeeded = false;
      await this.updateDPPX();
      await this.updateNetworkThrottling();
      reloadNeeded |= await this.updateUserAgent() &&
                      this.reloadOnChange("userAgent");
      reloadNeeded |= await this.updateTouchSimulation() &&
                      this.reloadOnChange("touchSimulation");
      if (reloadNeeded) {
        this.getViewportBrowser().reload();
      }
    }

    if (this.settingOnboardingTooltip) {
      this.settingOnboardingTooltip.destroy();
      this.settingOnboardingTooltip = null;
    }

    // Destroy local state
    const swap = this.swap;
    this.browserWindow = null;
    this.tab = null;
    this.inited = null;
    this.toolWindow = null;
    this.swap = null;

    // Close the debugger client used to speak with emulation actor.
    // The actor handles clearing any overrides itself, so it's not necessary to clear
    // anything on shutdown client side.
    const clientClosed = this.client.close();
    if (!isTabContentDestroying) {
      await clientClosed;
    }
    this.client = this.emulationFront = null;

    if (!isWindowClosing) {
      // Undo the swap and return the content back to a normal tab
      swap.stop();
    }

    this.destroyed = true;

    return true;
  },

  async connectToServer() {
    // The client being instantiated here is separate from the toolbox. It is being used
    // separately and has a life cycle that doesn't correspond to the toolbox. As a
    // result, it does not have a target, so we are not using `target.getFront` here. See
    // also the implementation for about:debugging
    DebuggerServer.init();
    DebuggerServer.registerAllActors();
    this.client = new DebuggerClient(DebuggerServer.connectPipe());
    await this.client.connect();
    const { tab } = await this.client.getTab();
    this.emulationFront = EmulationFront(this.client, tab);
  },

  /**
   * Show one-time notification about reloads for emulation.
   */
  showReloadNotification() {
    if (Services.prefs.getBoolPref(RELOAD_NOTIFICATION_PREF, false)) {
      showNotification(this.browserWindow, this.tab, {
        msg: l10n.getFormatStr("responsive.reloadNotification.description2"),
      });
      Services.prefs.setBoolPref(RELOAD_NOTIFICATION_PREF, false);
    }
  },

  reloadOnChange(id) {
    this.showReloadNotification();
    const pref = RELOAD_CONDITION_PREF_PREFIX + id;
    return Services.prefs.getBoolPref(pref, false);
  },

  handleEvent(event) {
    const { browserWindow, tab } = this;

    switch (event.type) {
      case "message":
        this.handleMessage(event);
        break;
      case "BeforeTabRemotenessChange":
      case "TabClose":
      case "unload":
        ResponsiveUIManager.closeIfNeeded(browserWindow, tab, {
          reason: event.type,
        });
        break;
    }
  },

  handleMessage(event) {
    if (event.origin !== "chrome://devtools") {
      return;
    }

    switch (event.data.type) {
      case "change-device":
        this.onChangeDevice(event);
        break;
      case "change-network-throttling":
        this.onChangeNetworkThrottling(event);
        break;
      case "change-pixel-ratio":
        this.onChangePixelRatio(event);
        break;
      case "change-touch-simulation":
        this.onChangeTouchSimulation(event);
        break;
      case "change-user-agent":
        this.onChangeUserAgent(event);
        break;
      case "content-resize":
        this.onContentResize(event);
        break;
      case "exit":
        this.onExit();
        break;
      case "remove-device-association":
        this.onRemoveDeviceAssociation();
        break;
      case "viewport-resize":
        this.onViewportResize(event);
        break;
    }
  },

  async onChangeDevice(event) {
    const { userAgent, pixelRatio, touch } = event.data.device;
    let reloadNeeded = false;
    await this.updateDPPX(pixelRatio);
    reloadNeeded |= await this.updateUserAgent(userAgent) &&
                    this.reloadOnChange("userAgent");
    reloadNeeded |= await this.updateTouchSimulation(touch) &&
                    this.reloadOnChange("touchSimulation");
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
    // Used by tests
    this.emit("device-changed");
  },

  async onChangeNetworkThrottling(event) {
    const { enabled, profile } = event.data;
    await this.updateNetworkThrottling(enabled, profile);
    // Used by tests
    this.emit("network-throttling-changed");
  },

  onChangePixelRatio(event) {
    const { pixelRatio } = event.data;
    this.updateDPPX(pixelRatio);
  },

  async onChangeTouchSimulation(event) {
    const { enabled } = event.data;
    const reloadNeeded = await this.updateTouchSimulation(enabled) &&
                         this.reloadOnChange("touchSimulation");
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
    // Used by tests
    this.emit("touch-simulation-changed");
  },

  async onChangeUserAgent(event) {
    const { userAgent } = event.data;
    const reloadNeeded = await this.updateUserAgent(userAgent) &&
                         this.reloadOnChange("userAgent");
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
    this.emit("user-agent-changed");
  },

  onContentResize(event) {
    const { width, height } = event.data;
    this.emit("content-resize", {
      width,
      height,
    });
  },

  onExit() {
    const { browserWindow, tab } = this;
    ResponsiveUIManager.closeIfNeeded(browserWindow, tab);
  },

  async onRemoveDeviceAssociation() {
    let reloadNeeded = false;
    await this.updateDPPX();
    reloadNeeded |= await this.updateUserAgent() &&
                    this.reloadOnChange("userAgent");
    reloadNeeded |= await this.updateTouchSimulation() &&
                    this.reloadOnChange("touchSimulation");
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
    // Used by tests
    this.emit("device-association-removed");
  },

  onViewportResize(event) {
    const { width, height } = event.data;
    this.emit("viewport-resize", {
      width,
      height,
    });
  },

  /**
   * Restores the previous state of RDM.
   */
  async restoreState() {
    const deviceState = await asyncStorage.getItem("devtools.responsive.deviceState");
    if (deviceState) {
      // Return if there is a device state to restore, this will be done when the
      // device list is loaded after the post-init.
      return;
    }

    const pixelRatio =
      Services.prefs.getIntPref("devtools.responsive.viewport.pixelRatio", 0);
    const touchSimulationEnabled =
      Services.prefs.getBoolPref("devtools.responsive.touchSimulation.enabled", false);
    const userAgent = Services.prefs.getCharPref("devtools.responsive.userAgent", "");

    let reloadNeeded = false;

    await this.updateDPPX(pixelRatio);

    if (touchSimulationEnabled) {
      reloadNeeded |= await this.updateTouchSimulation(touchSimulationEnabled) &&
                      this.reloadOnChange("touchSimulation");
    }
    if (userAgent) {
      reloadNeeded |= await this.updateUserAgent(userAgent) &&
                      this.reloadOnChange("userAgent");
    }
    if (reloadNeeded) {
      this.getViewportBrowser().reload();
    }
  },

  /**
   * Set or clear the emulated device pixel ratio.
   *
   * @return boolean
   *         Whether a reload is needed to apply the change.
   *         (This is always immediate, so it's always false.)
   */
  async updateDPPX(dppx) {
    if (!dppx) {
      await this.emulationFront.clearDPPXOverride();
      return false;
    }
    await this.emulationFront.setDPPXOverride(dppx);
    return false;
  },

  /**
   * Set or clear network throttling.
   *
   * @return boolean
   *         Whether a reload is needed to apply the change.
   *         (This is always immediate, so it's always false.)
   */
  async updateNetworkThrottling(enabled, profile) {
    if (!enabled) {
      await this.emulationFront.clearNetworkThrottling();
      return false;
    }
    const data = throttlingProfiles.find(({ id }) => id == profile);
    const { download, upload, latency } = data;
    await this.emulationFront.setNetworkThrottling({
      downloadThroughput: download,
      uploadThroughput: upload,
      latency,
    });
    return false;
  },

  /**
   * Set or clear the emulated user agent.
   *
   * @return boolean
   *         Whether a reload is needed to apply the change.
   */
  updateUserAgent(userAgent) {
    if (!userAgent) {
      return this.emulationFront.clearUserAgentOverride();
    }
    return this.emulationFront.setUserAgentOverride(userAgent);
  },

  /**
   * Set or clear touch simulation.
   *
   * @return boolean
   *         Whether a reload is needed to apply the change.
   */
  updateTouchSimulation(enabled) {
    let reloadNeeded;
    if (enabled) {
      reloadNeeded = this.emulationFront.setTouchEventsOverride(
        Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_ENABLED
      ).then(() => this.emulationFront.setMetaViewportOverride(
        Ci.nsIDocShell.META_VIEWPORT_OVERRIDE_ENABLED
      ));
    } else {
      reloadNeeded = this.emulationFront.clearTouchEventsOverride()
        .then(() => this.emulationFront.clearMetaViewportOverride());
    }
    return reloadNeeded;
  },

  /**
   * Helper for tests. Assumes a single viewport for now.
   */
  getViewportSize() {
    return this.toolWindow.getViewportSize();
  },

  /**
   * Helper for tests, etc. Assumes a single viewport for now.
   */
  async setViewportSize(size) {
    await this.inited;
    this.toolWindow.setViewportSize(size);
  },

  /**
   * Helper for tests/reloading the viewport. Assumes a single viewport for now.
   */
  getViewportBrowser() {
    return this.toolWindow.getViewportBrowser();
  },

  /**
   * Helper for contacting the viewport content. Assumes a single viewport for now.
   */
  getViewportMessageManager() {
    return this.getViewportBrowser().messageManager;
  },

};

EventEmitter.decorate(ResponsiveUI.prototype);
