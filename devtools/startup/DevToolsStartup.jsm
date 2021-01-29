/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This XPCOM component is loaded very early.
 * Be careful to lazy load dependencies as much as possible.
 *
 * It manages all the possible entry points for DevTools:
 * - Handles command line arguments like -jsconsole,
 * - Register all key shortcuts,
 * - Listen for "Web Developer" system menu opening, under "Tools",
 * - Inject the wrench icon in toolbar customization, which is used
 *   by the "Web Developer" list displayed in the hamburger menu,
 * - Register the JSON Viewer protocol handler.
 * - Inject the profiler recording button in toolbar customization.
 *
 * Only once any of these entry point is fired, this module ensures starting
 * core modules like 'devtools-browser.js' that hooks the browser windows
 * and ensure setting up tools.
 **/

"use strict";

const kDebuggerPrefs = [
  "devtools.debugger.remote-enabled",
  "devtools.chrome.enabled",
];

const DEVTOOLS_ENABLED_PREF = "devtools.enabled";
const DEVTOOLS_F12_DISABLED_PREF = "devtools.experiment.f12.shortcut_disabled";

const DEVTOOLS_POLICY_DISABLED_PREF = "devtools.policy.disabled";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { getConnectionStatus, setConnectionStatusChangeCallback } = ChromeUtils.import(
  "resource://devtools/server/actors/replay/connection.js"
);

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CustomizableWidgets",
  "resource:///modules/CustomizableWidgets.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ProfilerMenuButton",
  "resource://devtools/client/performance-new/popup/menu-button.jsm.js"
);
ChromeUtils.defineModuleGetter(
  this,
  "WebChannel",
  "resource://gre/modules/WebChannel.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PanelMultiView",
  "resource:///modules/PanelMultiView.jsm"
);

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const {
  ContentProcessListener,
} = require("devtools/server/actors/webconsole/listeners/content-process");

// We don't want to spend time initializing the full loader here so we create
// our own lazy require.
XPCOMUtils.defineLazyGetter(this, "Telemetry", function () {
  // eslint-disable-next-line no-shadow
  const Telemetry = require("devtools/client/shared/telemetry");

  return Telemetry;
});

XPCOMUtils.defineLazyGetter(this, "StartupBundle", function () {
  const url = "chrome://devtools-startup/locale/startup.properties";
  return Services.strings.createBundle(url);
});

XPCOMUtils.defineLazyGetter(this, "KeyShortcutsBundle", function () {
  const url = "chrome://devtools-startup/locale/key-shortcuts.properties";
  return Services.strings.createBundle(url);
});

const env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);

/**
 * Safely retrieve a localized DevTools key shortcut from KeyShortcutsBundle.
 * If the shortcut is not available, this will return null. Consumer code
 * should rely on this to skip unavailable shortcuts.
 *
 * Note that all shortcuts should always be available, but there is a notable
 * exception, which is why we have to do this. When a localization change is
 * uplifted to beta, language packs will not be updated immediately when the
 * updated beta is available.
 *
 * This means that language pack users might get a new Beta version but will not
 * have a language pack with the new strings yet.
 */
function getLocalizedKeyShortcut(id) {
  try {
    return KeyShortcutsBundle.GetStringFromName(id);
  } catch (e) {
    console.error("Failed to retrieve DevTools localized shortcut for id", id);
    return null;
  }
}

XPCOMUtils.defineLazyGetter(this, "KeyShortcuts", function () {
  const isMac = AppConstants.platform == "macosx";

  // Common modifier shared by most key shortcuts
  const modifiers = isMac ? "accel,alt" : "accel,shift";

  // List of all key shortcuts triggering installation UI
  // `id` should match tool's id from client/definitions.js
  const shortcuts = [
    // The following keys are also registered in /client/menus.js
    // And should be synced.

    // Both are toggling the toolbox on the last selected panel
    // or the default one.
    {
      id: "toggleToolbox",
      shortcut: getLocalizedKeyShortcut("toggleToolbox.commandkey"),
      modifiers,
    },
    // All locales are using F12
    {
      id: "toggleToolboxF12",
      shortcut: getLocalizedKeyShortcut("toggleToolboxF12.commandkey"),
      modifiers: "", // F12 is the only one without modifiers
    },
    // Open the Browser Toolbox
    {
      id: "browserToolbox",
      shortcut: getLocalizedKeyShortcut("browserToolbox.commandkey"),
      modifiers: "accel,alt,shift",
    },
    // Open the Browser Console
    {
      id: "browserConsole",
      shortcut: getLocalizedKeyShortcut("browserConsole.commandkey"),
      modifiers: "accel,shift",
    },
    // Toggle the Responsive Design Mode
    {
      id: "responsiveDesignMode",
      shortcut: getLocalizedKeyShortcut("responsiveDesignMode.commandkey"),
      modifiers,
    },
    // The following keys are also registered in /client/definitions.js
    // and should be synced.

    // Key for opening the Inspector
    {
      toolId: "inspector",
      shortcut: getLocalizedKeyShortcut("inspector.commandkey"),
      modifiers,
    },
    // Key for opening the Web Console
    {
      toolId: "webconsole",
      shortcut: getLocalizedKeyShortcut("webconsole.commandkey"),
      modifiers,
    },
    // Key for opening the Debugger
    {
      toolId: "jsdebugger",
      shortcut: getLocalizedKeyShortcut("jsdebugger.commandkey2"),
      modifiers,
    },
    // Key for opening the Network Monitor
    {
      toolId: "netmonitor",
      shortcut: getLocalizedKeyShortcut("netmonitor.commandkey"),
      modifiers,
    },
    // Key for opening the Style Editor
    {
      toolId: "styleeditor",
      shortcut: getLocalizedKeyShortcut("styleeditor.commandkey"),
      modifiers: "shift",
    },
    // Key for opening the Performance Panel
    {
      toolId: "performance",
      shortcut: getLocalizedKeyShortcut("performance.commandkey"),
      modifiers: "shift",
    },
    // Key for opening the Storage Panel
    {
      toolId: "storage",
      shortcut: getLocalizedKeyShortcut("storage.commandkey"),
      modifiers: "shift",
    },
    // Key for opening the DOM Panel
    {
      toolId: "dom",
      shortcut: getLocalizedKeyShortcut("dom.commandkey"),
      modifiers,
    },
    // Key for opening the Accessibility Panel
    {
      toolId: "accessibility",
      shortcut: getLocalizedKeyShortcut("accessibilityF12.commandkey"),
      modifiers: "shift",
    },
  ];

  if (isMac) {
    // Add the extra key command for macOS, so you can open the inspector with cmd+shift+C
    // like on Chrome DevTools.
    shortcuts.push({
      id: "inspectorMac",
      toolId: "inspector",
      shortcut: getLocalizedKeyShortcut("inspector.commandkey"),
      modifiers: "accel,shift",
    });
  }

  if (ProfilerMenuButton.isInNavbar()) {
    shortcuts.push(...getProfilerKeyShortcuts());
  }

  return shortcuts;
});

function getProfilerKeyShortcuts() {
  return [
    // Start/stop the profiler
    {
      id: "profilerStartStop",
      shortcut: getLocalizedKeyShortcut("profilerStartStop.commandkey"),
      modifiers: "control,shift",
    },
    // Capture a profile
    {
      id: "profilerCapture",
      shortcut: getLocalizedKeyShortcut("profilerCapture.commandkey"),
      modifiers: "control,shift",
    },
  ];
}

/**
 * Validate the URL that will be used for the WebChannel for the profiler.
 *
 * @param {string} targetUrl
 * @returns {string}
 */
function validateProfilerWebChannelUrl(targetUrl) {
  const frontEndUrl = "https://profiler.firefox.com";

  if (targetUrl !== frontEndUrl) {
    // The user can specify either localhost or deploy previews as well as
    // the official frontend URL for testing.
    if (
      // Allow a test URL.
      targetUrl === "http://example.com" ||
      // Allows the following:
      //   "http://localhost:4242"
      //   "http://localhost:4242/"
      //   "http://localhost:3"
      //   "http://localhost:334798455"
      /^http:\/\/localhost:\d+\/?$/.test(targetUrl) ||
      // Allows the following:
      //   "https://deploy-preview-1234--perf-html.netlify.com"
      //   "https://deploy-preview-1234--perf-html.netlify.com/"
      //   "https://deploy-preview-1234567--perf-html.netlify.app"
      //   "https://main--perf-html.netlify.app"
      /^https:\/\/(?:deploy-preview-\d+|main)--perf-html\.netlify\.(?:com|app)\/?$/.test(
        targetUrl
      )
    ) {
      // This URL is one of the allowed ones to be used for configuration.
      return targetUrl;
    }

    console.error(
      `The preference "devtools.performance.recording.ui-base-url" was set to a ` +
        "URL that is not allowed. No WebChannel messages will be sent between the " +
        `browser and that URL. Falling back to ${frontEndUrl}. Only localhost ` +
        "and deploy previews URLs are allowed.",
      targetUrl
    );
  }

  return frontEndUrl;
}

XPCOMUtils.defineLazyGetter(this, "ProfilerPopupBackground", function () {
  return ChromeUtils.import(
    "resource://devtools/client/performance-new/popup/background.jsm.js"
  );
});

function DevToolsStartup() {
  this.onEnabledPrefChanged = this.onEnabledPrefChanged.bind(this);
  this.onWindowReady = this.onWindowReady.bind(this);
  this.toggleProfilerKeyShortcuts = this.toggleProfilerKeyShortcuts.bind(this);
}

DevToolsStartup.prototype = {
  /**
   * Boolean flag to check if DevTools have been already initialized or not.
   * By initialized, we mean that its main modules are loaded.
   */
  initialized: false,

  /**
   * Boolean flag to check if the devtools initialization was already sent to telemetry.
   * We only want to record one devtools entry point per Firefox run, but we are not
   * interested in all the entry points.
   */
  recorded: false,

  get telemetry() {
    if (!this._telemetry) {
      this._telemetry = new Telemetry();
      this._telemetry.setEventRecordingEnabled(true);
    }
    return this._telemetry;
  },

  /**
   * Flag that indicates if the developer toggle was already added to customizableUI.
   */
  developerToggleCreated: false,

  recordingButtonCreated: false,

  /**
   * Flag that indicates if the profiler recording popup was already added to
   * customizableUI.
   */
  profilerRecordingButtonCreated: false,

  isDisabledByPolicy: function () {
    return Services.prefs.getBoolPref(DEVTOOLS_POLICY_DISABLED_PREF, false);
  },

  onConsoleAPICall: (message) => {
    console.log(...message.arguments);
  },

  handle: function (cmdLine) {
    const flags = this.readCommandLineFlags(cmdLine);

    // handle() can be called after browser startup (e.g. opening links from other apps).
    const isInitialLaunch =
      cmdLine.state == Ci.nsICommandLine.STATE_INITIAL_LAUNCH;
    if (isInitialLaunch) {
      // Enable devtools for all users on startup (onboarding experiment from Bug 1408969
      // is over).
      Services.prefs.setBoolPref(DEVTOOLS_ENABLED_PREF, true);

      // The F12 shortcut might be disabled to avoid accidental usage.
      // Users who are already considered as devtools users should not be
      // impacted.
      if (this.isDevToolsUser()) {
        Services.prefs.setBoolPref(DEVTOOLS_F12_DISABLED_PREF, false);
      }

      // Store devtoolsFlag to check it later in onWindowReady.
      this.devtoolsFlag = flags.devtools;

      /* eslint-disable mozilla/balanced-observers */
      // We are not expecting to remove those listeners until Firefox closes.

      // Only top level Firefox Windows fire a browser-delayed-startup-finished event
      Services.obs.addObserver(
        this.onWindowReady,
        "browser-delayed-startup-finished"
      );

      // Update menu items when devtools.enabled changes.
      Services.prefs.addObserver(
        DEVTOOLS_ENABLED_PREF,
        this.onEnabledPrefChanged
      );
      /* eslint-enable mozilla/balanced-observers */

      if (!this.isDisabledByPolicy()) {
        if (AppConstants.MOZ_DEV_EDITION) {
          // On DevEdition, the developer toggle is displayed by default in the navbar
          // area and should be created before the first paint.
          this.hookDeveloperToggle();
        }

        this.hookRecordingButton();
        this.hookProfilerRecordingButton();
      }

      if (isRunningTest()) {
        new ContentProcessListener(this.onConsoleAPICall);
      }
    }

    if (flags.console) {
      this.commandLine = true;
      this.handleConsoleFlag(cmdLine);
    }
    if (flags.debugger) {
      this.commandLine = true;
      const binaryPath =
        typeof flags.debugger == "string" ? flags.debugger : null;
      this.handleDebuggerFlag(cmdLine, binaryPath);
    }

    if (flags.devToolsServer) {
      this.handleDevToolsServerFlag(cmdLine, flags.devToolsServer);
    }
  },

  readCommandLineFlags(cmdLine) {
    // All command line flags are disabled if DevTools are disabled by policy.
    if (this.isDisabledByPolicy()) {
      return {
        console: false,
        debugger: false,
        devtools: false,
        devToolsServer: false,
      };
    }

    const console = cmdLine.handleFlag("jsconsole", false);
    const devtools = cmdLine.handleFlag("devtools", false);

    let devToolsServer;
    try {
      devToolsServer = cmdLine.handleFlagWithParam(
        "start-debugger-server",
        false
      );
    } catch (e) {
      // We get an error if the option is given but not followed by a value.
      // By catching and trying again, the value is effectively optional.
      devToolsServer = cmdLine.handleFlag("start-debugger-server", false);
    }

    let debuggerFlag;
    try {
      debuggerFlag = cmdLine.handleFlagWithParam("jsdebugger", false);
    } catch (e) {
      // We get an error if the option is given but not followed by a value.
      // By catching and trying again, the value is effectively optional.
      debuggerFlag = cmdLine.handleFlag("jsdebugger", false);
    }

    return { console, debugger: debuggerFlag, devtools, devToolsServer };
  },

  /**
   * Called when receiving the "browser-delayed-startup-finished" event for a new
   * top-level window.
   */
  onWindowReady(window) {
    if (this.isDisabledByPolicy()) {
      this.removeDevToolsMenus(window);
      return;
    }

    this.hookWindow(window);

    // This listener is called for all Firefox windows, but we want to execute some code
    // only once.
    if (!this._firstWindowReadyReceived) {
      this.onFirstWindowReady(window);
      this._firstWindowReadyReceived = true;
    }

    JsonView.initialize();
  },

  removeDevToolsMenus(window) {
    // This will hide the "Tools > Web Developer" menu.
    window.document
      .getElementById("webDeveloperMenu")
      .setAttribute("hidden", "true");
    // This will hide the "Web Developer" item in the hamburger menu.
    PanelMultiView.getViewNode(
      window.document,
      "appMenu-developer-button"
    ).setAttribute("hidden", "true");
  },

  onFirstWindowReady(window) {
    if (this.devtoolsFlag) {
      this.handleDevToolsFlag(window);

      // In the case of the --jsconsole and --jsdebugger command line parameters
      // there was no browser window when they were processed so we act on the
      // this.commandline flag instead.
      if (this.commandLine) {
        this.sendEntryPointTelemetry("CommandLine");
      }
    }
  },

  /**
   * Register listeners to all possible entry points for Developer Tools.
   * But instead of implementing the actual actions, defer to DevTools codebase.
   * In most cases, it only needs to call this.initDevTools which handles the rest.
   * We do that to prevent loading any DevTools module until the user intent to use them.
   */
  hookWindow(window) {
    // Key Shortcuts need to be added on all the created windows.
    this.hookKeyShortcuts(window);

    // In some situations (e.g. starting Firefox with --jsconsole) DevTools will be
    // initialized before the first browser-delayed-startup-finished event is received.
    // We use a dedicated flag because we still need to hook the developer toggle.
    this.hookDeveloperToggle();
    this.hookRecordingButton();
    this.hookProfilerRecordingButton();

    // The developer menu hook only needs to be added if devtools have not been
    // initialized yet.
    if (!this.initialized) {
      this.hookWebDeveloperMenu(window);
    }

    this.createDevToolsEnableMenuItem(window);
    this.updateDevToolsMenuItems(window);
  },

  /**
   * Dynamically register a wrench icon in the customization menu.
   * You can use this button by right clicking on Firefox toolbar
   * and dragging it from the customization panel to the toolbar.
   * (i.e. this isn't displayed by default to users!)
   *
   * _But_, the "Web Developer" entry in the hamburger menu (the menu with
   * 3 horizontal lines), is using this "developer-button" view to populate
   * its menu. So we have to register this button for the menu to work.
   *
   * Also, this menu duplicates its own entries from the "Web Developer"
   * menu in the system menu, under "Tools" main menu item. The system
   * menu is being hooked by "hookWebDeveloperMenu" which ends up calling
   * devtools/client/framework/browser-menus to create the items for real,
   * initDevTools, from onViewShowing is also calling browser-menu.
   */
  hookDeveloperToggle() {
    if (this.developerToggleCreated) {
      return;
    }

    const id = "developer-button";
    const widget = CustomizableUI.getWidget(id);
    if (widget && widget.provider == CustomizableUI.PROVIDER_API) {
      return;
    }
    const item = {
      id: id,
      type: "view",
      viewId: "PanelUI-developer",
      shortcutId: "key_toggleToolbox",
      tooltiptext: "developer-button.tooltiptext2",
      onViewShowing: (event) => {
        if (Services.prefs.getBoolPref(DEVTOOLS_ENABLED_PREF)) {
          // If DevTools are enabled, initialize DevTools to create all menuitems in the
          // system menu before trying to copy them.
          this.initDevTools("HamburgerMenu");
        }

        // Populate the subview with whatever menuitems are in the developer
        // menu. We skip menu elements, because the menu panel has no way
        // of dealing with those right now.
        const doc = event.target.ownerDocument;

        const menu = doc.getElementById("menuWebDeveloperPopup");

        const itemsToDisplay = [...menu.children];
        // Hardcode the addition of the "work offline" menuitem at the bottom:
        itemsToDisplay.push({
          localName: "menuseparator",
          getAttribute: () => {},
        });
        itemsToDisplay.push(doc.getElementById("goOfflineMenuitem"));

        const developerItems = PanelMultiView.getViewNode(
          doc,
          "PanelUI-developerItems"
        );
        CustomizableUI.clearSubview(developerItems);
        CustomizableUI.fillSubviewFromMenuItems(itemsToDisplay, developerItems);
      },
      onInit(anchor) {
        // Since onBeforeCreated already bails out when initialized, we can call
        // it right away.
        this.onBeforeCreated(anchor.ownerDocument);
      },
      onBeforeCreated: (doc) => {
        // The developer toggle needs the "key_toggleToolbox" <key> element.
        // In DEV EDITION, the toggle is added before 1st paint and hookKeyShortcuts() is
        // not called yet when CustomizableUI creates the widget.
        this.hookKeyShortcuts(doc.defaultView);

        if (PanelMultiView.getViewNode(doc, "PanelUI-developerItems")) {
          return;
        }
        const view = doc.createXULElement("panelview");
        view.id = "PanelUI-developerItems";
        const panel = doc.createXULElement("vbox");
        panel.setAttribute("class", "panel-subview-body");
        view.appendChild(panel);
        doc.getElementById("PanelUI-multiView").appendChild(view);
      },
    };
    CustomizableUI.createWidget(item);
    CustomizableWidgets.push(item);

    this.developerToggleCreated = true;
  },

  hookRecordingButton() {
    if (this.recordingButtonCreated) {
      return;
    }
    this.recordingButtonCreated = true;
    this.initializeRecordingWebChannel();

    createRecordingButton();
  },

  /**
   * Register the profiler recording button. This button will be available
   * in the customization palette for the Firefox toolbar. In addition, it can be
   * enabled from profiler.firefox.com.
   */
  hookProfilerRecordingButton() {
    if (this.profilerRecordingButtonCreated) {
      return;
    }
    const featureFlagPref = "devtools.performance.popup.feature-flag";
    const isPopupFeatureFlagEnabled = Services.prefs.getBoolPref(
      featureFlagPref
    );
    this.profilerRecordingButtonCreated = true;

    // Listen for messages from the front-end. This needs to happen even if the
    // button isn't enabled yet. This will allow the front-end to turn on the
    // popup for our users, regardless of if the feature is enabled by default.
    this.initializeProfilerWebChannel();

    if (isPopupFeatureFlagEnabled) {
      // Initialize the CustomizableUI widget.
      ProfilerMenuButton.initialize(this.toggleProfilerKeyShortcuts);
    } else {
      // The feature flag is not enabled, but watch for it to be enabled. If it is,
      // initialize everything.
      const enable = () => {
        ProfilerMenuButton.initialize(this.toggleProfilerKeyShortcuts);
        Services.prefs.removeObserver(featureFlagPref, enable);
      };
      Services.prefs.addObserver(featureFlagPref, enable);
    }
  },

  /**
   * Initialize the WebChannel for profiler.firefox.com. This function happens at
   * startup, so care should be taken to minimize its performance impact. The WebChannel
   * is a mechanism that is used to communicate between the browser, and front-end code.
   */
  initializeProfilerWebChannel() {
    let channel;

    // Register a channel for the URL in preferences. Also update the WebChannel if
    // the URL changes.
    const urlPref = "devtools.performance.recording.ui-base-url";

    // This method is only run once per Firefox instance, so it should not be
    // strictly necessary to remove observers here.
    // eslint-disable-next-line mozilla/balanced-observers
    Services.prefs.addObserver(urlPref, registerWebChannel);

    registerWebChannel();

    function registerWebChannel() {
      if (channel) {
        channel.stopListening();
      }

      const urlForWebChannel = Services.io.newURI(
        validateProfilerWebChannelUrl(Services.prefs.getStringPref(urlPref))
      );

      channel = new WebChannel("profiler.firefox.com", urlForWebChannel);

      channel.listen((id, message, target) => {
        // Defer loading the ProfilerPopupBackground script until it's absolutely needed,
        // as this code path gets loaded at startup.
        ProfilerPopupBackground.handleWebChannelMessage(
          channel,
          id,
          message,
          target
        );
      });
    }
  },

  initializeRecordingWebChannel() {
    const pageUrl = Services.prefs.getStringPref(
      "devtools.recordreplay.recordingsUrl"
    );
    const localUrl = "http://localhost:8080/view";

    registerWebChannel(pageUrl);
    registerWebChannel(localUrl);

    function registerWebChannel(url) {
      const urlForWebChannel = Services.io.newURI(url);
      const channel = new WebChannel("record-replay", urlForWebChannel);

      channel.listen((id, message) => {
        const { user } = message;
        saveRecordingUser(user);
      });
    }
  },
  /*
   * We listen to the "Web Developer" system menu, which is under "Tools" main item.
   * This menu item is hardcoded empty in Firefox UI. We listen for its opening to
   * populate it lazily. Loading main DevTools module is going to populate it.
   */
  hookWebDeveloperMenu(window) {
    const menu = window.document.getElementById("webDeveloperMenu");
    const onPopupShowing = () => {
      if (!Services.prefs.getBoolPref(DEVTOOLS_ENABLED_PREF)) {
        return;
      }
      menu.removeEventListener("popupshowing", onPopupShowing);
      this.initDevTools("SystemMenu");
    };
    menu.addEventListener("popupshowing", onPopupShowing);
  },

  /**
   * Create a new menu item to enable DevTools and insert it DevTools's submenu in the
   * System Menu.
   */
  createDevToolsEnableMenuItem(window) {
    const { document } = window;

    // Create the menu item.
    const item = document.createXULElement("menuitem");
    item.id = "enableDeveloperTools";
    item.setAttribute(
      "label",
      StartupBundle.GetStringFromName("enableDevTools.label")
    );
    item.setAttribute(
      "accesskey",
      StartupBundle.GetStringFromName("enableDevTools.accesskey")
    );

    // The menu item should open the install page for DevTools.
    item.addEventListener("command", () => {
      this.openInstallPage("SystemMenu");
    });

    // Insert the menu item in the DevTools submenu.
    const systemMenuItem = document.getElementById("menuWebDeveloperPopup");
    systemMenuItem.appendChild(item);
  },

  /**
   * Update the visibility the menu item to enable DevTools.
   */
  updateDevToolsMenuItems(window) {
    const item = window.document.getElementById("enableDeveloperTools");
    item.hidden = Services.prefs.getBoolPref(DEVTOOLS_ENABLED_PREF);
  },

  /**
   * Loop on all windows and update the hidden attribute of the "enable DevTools" menu
   * item.
   */
  onEnabledPrefChanged() {
    for (const window of Services.wm.getEnumerator("navigator:browser")) {
      if (window.gBrowserInit && window.gBrowserInit.delayedStartupFinished) {
        this.updateDevToolsMenuItems(window);
      }
    }
  },

  /**
   * Check if the user is a DevTools user by looking at our selfxss pref.
   * This preference is incremented everytime the console is used (up to 5).
   *
   * @return {Boolean} true if the user can be considered as a devtools user.
   */
  isDevToolsUser() {
    const selfXssCount = Services.prefs.getIntPref("devtools.selfxss.count", 0);
    return selfXssCount > 0;
  },

  hookKeyShortcuts(window) {
    const doc = window.document;

    // hookKeyShortcuts can be called both from hookWindow and from the developer toggle
    // onBeforeCreated. Make sure shortcuts are only added once per window.
    if (doc.getElementById("devtoolsKeyset")) {
      return;
    }

    const keyset = doc.createXULElement("keyset");
    keyset.setAttribute("id", "devtoolsKeyset");

    this.attachKeys(doc, KeyShortcuts, keyset);

    // Appending a <key> element is not always enough. The <keyset> needs
    // to be detached and reattached to make sure the <key> is taken into
    // account (see bug 832984).
    const mainKeyset = doc.getElementById("mainKeyset");
    mainKeyset.parentNode.insertBefore(keyset, mainKeyset);
  },

  /**
   * This method attaches on the key elements to the devtools keyset.
   */
  attachKeys(doc, keyShortcuts, keyset = doc.getElementById("devtoolsKeyset")) {
    const window = doc.defaultView;
    for (const key of keyShortcuts) {
      if (!key.shortcut) {
        // Shortcuts might be missing when a user relies on a language packs
        // which is missing a recently uplifted shortcut. Language packs are
        // typically updated a few days after a code uplift.
        continue;
      }
      const xulKey = this.createKey(doc, key, () => this.onKey(window, key));
      keyset.appendChild(xulKey);
    }
  },

  /**
   * This method removes keys from the devtools keyset.
   */
  removeKeys(doc, keyShortcuts) {
    for (const key of keyShortcuts) {
      const keyElement = doc.getElementById(this.getKeyElementId(key));
      if (keyElement) {
        keyElement.remove();
      }
    }
  },

  /**
   * We only want to have the keyboard shortcuts active when the menu button is on.
   * This function either adds or removes the elements.
   * @param {boolean} isEnabled
   */
  toggleProfilerKeyShortcuts(isEnabled) {
    const profilerKeyShortcuts = getProfilerKeyShortcuts();
    for (const { document } of Services.wm.getEnumerator(null)) {
      const devtoolsKeyset = document.getElementById("devtoolsKeyset");
      const mainKeyset = document.getElementById("mainKeyset");

      if (!devtoolsKeyset || !mainKeyset) {
        // There may not be devtools keyset on this window.
        continue;
      }

      const areProfilerKeysPresent = !!document.getElementById(
        "key_profilerStartStop"
      );
      if (isEnabled === areProfilerKeysPresent) {
        // Don't double add or double remove the shortcuts.
        continue;
      }
      if (isEnabled) {
        this.attachKeys(document, profilerKeyShortcuts);
      } else {
        this.removeKeys(document, profilerKeyShortcuts);
      }
      // Appending a <key> element is not always enough. The <keyset> needs
      // to be detached and reattached to make sure the <key> is taken into
      // account (see bug 832984).
      mainKeyset.parentNode.insertBefore(devtoolsKeyset, mainKeyset);
    }
  },

  async onKey(window, key) {
    try {
      // The profiler doesn't care if DevTools is loaded, so provide a quick check
      // first to bail out of checking if DevTools is available.
      switch (key.id) {
        case "profilerStartStop": {
          ProfilerPopupBackground.toggleProfiler("aboutprofiling");
          return;
        }
        case "profilerCapture": {
          ProfilerPopupBackground.captureProfile("aboutprofiling");
          return;
        }
      }
      if (!Services.prefs.getBoolPref(DEVTOOLS_ENABLED_PREF)) {
        const id = key.toolId || key.id;
        this.openInstallPage("KeyShortcut", id);
      } else {
        // Record the timing at which this event started in order to compute later in
        // gDevTools.showToolbox, the complete time it takes to open the toolbox.
        // i.e. especially take `initDevTools` into account.
        const startTime = Cu.now();
        const require = this.initDevTools("KeyShortcut", key);
        const {
          gDevToolsBrowser,
        } = require("devtools/client/framework/devtools-browser");
        await gDevToolsBrowser.onKeyShortcut(window, key, startTime);
      }
    } catch (e) {
      console.error(`Exception while trigerring key ${key}: ${e}\n${e.stack}`);
    }
  },

  getKeyElementId({ id, toolId }) {
    return "key_" + (id || toolId);
  },

  // Create a <xul:key> DOM Element
  createKey(doc, key, oncommand) {
    const { shortcut, modifiers: mod } = key;
    const k = doc.createXULElement("key");
    k.id = this.getKeyElementId(key);

    if (shortcut.startsWith("VK_")) {
      k.setAttribute("keycode", shortcut);
      if (shortcut.match(/^VK_\d$/)) {
        // Add the event keydown attribute to ensure that shortcuts work for combinations
        // such as ctrl shift 1.
        k.setAttribute("event", "keydown");
      }
    } else {
      k.setAttribute("key", shortcut);
    }

    if (mod) {
      k.setAttribute("modifiers", mod);
    }

    // Bug 371900: command event is fired only if "oncommand" attribute is set.
    k.setAttribute("oncommand", ";");
    k.addEventListener("command", oncommand);

    return k;
  },

  initDevTools: function (reason, key = "") {
    // If an entry point is fired and tools are not enabled open the installation page
    if (!Services.prefs.getBoolPref(DEVTOOLS_ENABLED_PREF)) {
      this.openInstallPage(reason);
      return null;
    }

    // In the case of the --jsconsole and --jsdebugger command line parameters
    // there is no browser window yet so we don't send any telemetry yet.
    if (reason !== "CommandLine") {
      this.sendEntryPointTelemetry(reason, key);
    }

    this.initialized = true;
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );
    // Ensure loading main devtools module that hooks up into browser UI
    // and initialize all devtools machinery.
    require("devtools/client/framework/devtools-browser");
    return require;
  },

  /**
   * Open about:devtools to start the onboarding flow.
   *
   * @param {String} reason
   *        One of "KeyShortcut", "SystemMenu", "HamburgerMenu", "ContextMenu",
   *        "CommandLine".
   * @param {String} keyId
   *        Optional. If the onboarding flow was triggered by a keyboard shortcut, pass
   *        the shortcut key id (or toolId) to about:devtools.
   */
  openInstallPage: function (reason, keyId) {
    // If DevTools are completely disabled, bail out here as this might be called directly
    // from other files.
    if (this.isDisabledByPolicy()) {
      return;
    }

    const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");

    // Focus about:devtools tab if there is already one opened in the current window.
    for (const tab of gBrowser.tabs) {
      const browser = tab.linkedBrowser;
      // browser.documentURI might be undefined if the browser tab is still loading.
      const location = browser.documentURI ? browser.documentURI.spec : "";
      if (
        location.startsWith("about:devtools") &&
        !location.startsWith("about:devtools-toolbox")
      ) {
        // Focus the existing about:devtools tab and bail out.
        gBrowser.selectedTab = tab;
        return;
      }
    }

    let url = "about:devtools";

    const params = [];
    if (reason) {
      params.push("reason=" + encodeURIComponent(reason));
    }

    const selectedBrowser = gBrowser.selectedBrowser;
    if (selectedBrowser) {
      params.push("tabid=" + selectedBrowser.outerWindowID);
    }

    if (keyId) {
      params.push("keyid=" + keyId);
    }

    if (params.length > 0) {
      url += "?" + params.join("&");
    }

    // Set relatedToCurrent: true to open the tab next to the current one.
    gBrowser.selectedTab = gBrowser.addTrustedTab(url, {
      relatedToCurrent: true,
    });
  },

  handleConsoleFlag: function (cmdLine) {
    const window = Services.wm.getMostRecentWindow("devtools:webconsole");
    if (!window) {
      const require = this.initDevTools("CommandLine");
      const {
        BrowserConsoleManager,
      } = require("devtools/client/webconsole/browser-console-manager");
      BrowserConsoleManager.toggleBrowserConsole().catch(console.error);
    } else {
      // the Browser Console was already open
      window.focus();
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  // Open the toolbox on the selected tab once the browser starts up.
  handleDevToolsFlag: async function (window) {
    const require = this.initDevTools("CommandLine");
    const { gDevTools } = require("devtools/client/framework/devtools");
    const { TargetFactory } = require("devtools/client/framework/target");
    const target = await TargetFactory.forTab(window.gBrowser.selectedTab);
    gDevTools.showToolbox(target);
  },

  _isRemoteDebuggingEnabled() {
    let remoteDebuggingEnabled = false;
    try {
      remoteDebuggingEnabled = kDebuggerPrefs.every((pref) => {
        return Services.prefs.getBoolPref(pref);
      });
    } catch (ex) {
      console.error(ex);
      return false;
    }
    if (!remoteDebuggingEnabled) {
      const errorMsg =
        "Could not run chrome debugger! You need the following " +
        "prefs to be set to true: " +
        kDebuggerPrefs.join(", ");
      console.error(new Error(errorMsg));
      // Dump as well, as we're doing this from a commandline, make sure people
      // don't miss it:
      dump(errorMsg + "\n");
    }
    return remoteDebuggingEnabled;
  },

  handleDebuggerFlag: function (cmdLine, binaryPath) {
    if (!this._isRemoteDebuggingEnabled()) {
      return;
    }

    let devtoolsThreadResumed = false;
    const pauseOnStartup = cmdLine.handleFlag("wait-for-jsdebugger", false);
    if (pauseOnStartup) {
      const observe = function (subject, topic, data) {
        devtoolsThreadResumed = true;
        Services.obs.removeObserver(observe, "devtools-thread-resumed");
      };
      Services.obs.addObserver(observe, "devtools-thread-resumed");
    }

    const { BrowserToolboxLauncher } = ChromeUtils.import(
      "resource://devtools/client/framework/browser-toolbox/Launcher.jsm"
    );
    BrowserToolboxLauncher.init(null, null, null, binaryPath);

    if (pauseOnStartup) {
      // Spin the event loop until the debugger connects.
      const tm = Cc["@mozilla.org/thread-manager;1"].getService();
      tm.spinEventLoopUntil(() => {
        return devtoolsThreadResumed;
      });
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  /**
   * Handle the --start-debugger-server command line flag. The options are:
   * --start-debugger-server
   *   The portOrPath parameter is boolean true in this case. Reads and uses the defaults
   *   from devtools.debugger.remote-port and devtools.debugger.remote-websocket prefs.
   *   The default values of these prefs are port 6000, WebSocket disabled.
   *
   * --start-debugger-server 6789
   *   Start the non-WebSocket server on port 6789.
   *
   * --start-debugger-server /path/to/filename
   *   Start the server on a Unix domain socket.
   *
   * --start-debugger-server ws:6789
   *   Start the WebSocket server on port 6789.
   *
   * --start-debugger-server ws:
   *   Start the WebSocket server on the default port (taken from d.d.remote-port)
   */
  handleDevToolsServerFlag: function (cmdLine, portOrPath) {
    if (!this._isRemoteDebuggingEnabled()) {
      return;
    }

    let webSocket = false;
    const defaultPort = Services.prefs.getIntPref(
      "devtools.debugger.remote-port"
    );
    if (portOrPath === true) {
      // Default to pref values if no values given on command line
      webSocket = Services.prefs.getBoolPref(
        "devtools.debugger.remote-websocket"
      );
      portOrPath = defaultPort;
    } else if (portOrPath.startsWith("ws:")) {
      webSocket = true;
      const port = portOrPath.slice(3);
      portOrPath = Number(port) ? port : defaultPort;
    }

    const { DevToolsLoader } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );

    try {
      // Create a separate loader instance, so that we can be sure to receive
      // a separate instance of the DebuggingServer from the rest of the
      // devtools.  This allows us to safely use the tools against even the
      // actors and DebuggingServer itself, especially since we can mark
      // serverLoader as invisible to the debugger (unlike the usual loader
      // settings).
      const serverLoader = new DevToolsLoader({
        invisibleToDebugger: true,
      });
      const { DevToolsServer: devToolsServer } = serverLoader.require(
        "devtools/server/devtools-server"
      );
      const { SocketListener } = serverLoader.require(
        "devtools/shared/security/socket"
      );
      devToolsServer.init();
      devToolsServer.registerAllActors();
      devToolsServer.allowChromeProcess = true;
      const socketOptions = { portOrPath, webSocket };

      const listener = new SocketListener(devToolsServer, socketOptions);
      listener.open();
      dump("Started devtools server on " + portOrPath + "\n");
    } catch (e) {
      dump("Unable to start devtools server on " + portOrPath + ": " + e);
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO) {
      cmdLine.preventDefault = true;
    }
  },

  /**
   * Send entry point telemetry explaining how the devtools were launched. This
   * functionality also lives inside `devtools/client/framework/browser-menus.js`
   * because this codepath is only used the first time a toolbox is opened for a
   * tab.
   *
   * @param {String} reason
   *        One of "KeyShortcut", "SystemMenu", "HamburgerMenu", "ContextMenu",
   *        "CommandLine".
   * @param {String} key
   *        The key used by a key shortcut.
   */
  sendEntryPointTelemetry(reason, key = "") {
    if (!reason) {
      return;
    }

    let keys = "";

    if (reason === "KeyShortcut") {
      let { modifiers, shortcut } = key;

      modifiers = modifiers.replace(",", "+");

      if (shortcut.startsWith("VK_")) {
        shortcut = shortcut.substr(3);
      }

      keys = `${modifiers}+${shortcut}`;
    }

    const window = Services.wm.getMostRecentWindow("navigator:browser");

    this.telemetry.addEventProperty(
      window,
      "open",
      "tools",
      null,
      "shortcut",
      keys
    );
    this.telemetry.addEventProperty(
      window,
      "open",
      "tools",
      null,
      "entrypoint",
      reason
    );

    if (this.recorded) {
      return;
    }

    // Only save the first call for each firefox run as next call
    // won't necessarely start the tool. For example key shortcuts may
    // only change the currently selected tool.
    try {
      this.telemetry.getHistogramById("DEVTOOLS_ENTRY_POINT").add(reason);
    } catch (e) {
      dump("DevTools telemetry entry point failed: " + e + "\n");
    }
    this.recorded = true;
  },

  // Used by tests and the toolbox to register the same key shortcuts in toolboxes loaded
  // in a window window.
  get KeyShortcuts() {
    return KeyShortcuts;
  },
  get wrappedJSObject() {
    return this;
  },

  /* eslint-disable max-len */
  helpInfo:
    "  --jsconsole        Open the Browser Console.\n" +
    "  --jsdebugger [<path>] Open the Browser Toolbox. Defaults to the local build\n" +
    "                     but can be overridden by a firefox path.\n" +
    "  --wait-for-jsdebugger Spin event loop until JS debugger connects.\n" +
    "                     Enables debugging (some) application startup code paths.\n" +
    "                     Only has an effect when `--jsdebugger` is also supplied.\n" +
    "  --devtools         Open DevTools on initial load.\n" +
    "  --start-debugger-server [ws:][ <port> | <path> ] Start the devtools server on\n" +
    "                     a TCP port or Unix domain socket path. Defaults to TCP port\n" +
    "                     6000. Use WebSocket protocol if ws: prefix is specified.\n",
  /* eslint-disable max-len */

  classID: Components.ID("{9e9a9283-0ce9-4e4a-8f1c-ba129a032c32}"),
  QueryInterface: ChromeUtils.generateQI(["nsICommandLineHandler"]),
};

/**
 * Singleton object that represents the JSON View in-content tool.
 * It has the same lifetime as the browser.
 */
const JsonView = {
  initialized: false,

  initialize: function () {
    // Prevent loading the frame script multiple times if we call this more than once.
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    // Register for messages coming from the child process.
    // This is never removed as there is no particular need to unregister
    // it during shutdown.
    Services.mm.addMessageListener("devtools:jsonview:save", this.onSave);
  },

  // Message handlers for events from child processes

  /**
   * Save JSON to a file needs to be implemented here
   * in the parent process.
   */
  onSave: function (message) {
    const browser = message.target;
    const chrome = browser.ownerGlobal;
    if (message.data === null) {
      // Save original contents
      chrome.saveBrowser(browser);
    } else {
      if (
        !message.data.startsWith("blob:null") ||
        !browser.contentPrincipal.isNullPrincipal
      ) {
        Cu.reportError("Got invalid request to save JSON data");
        return;
      }
      // The following code emulates saveBrowser, but:
      // - Uses the given blob URL containing the custom contents to save.
      // - Obtains the file name from the URL of the document, not the blob.
      // - avoids passing the document and explicitly passes system principal.
      //   We have a blob created by a null principal to save, and the null
      //   principal is from the child. Null principals don't survive crossing
      //   over IPC, so there's no other principal that'll work.
      const persistable = browser.frameLoader;
      persistable.startPersistence(null, {
        onDocumentReady(doc) {
          const uri = chrome.makeURI(doc.documentURI, doc.characterSet);
          const filename = chrome.getDefaultFileName(undefined, uri, doc, null);
          chrome.internalSave(
            message.data,
            null,
            filename,
            null,
            doc.contentType,
            false /* bypass cache */,
            null /* filepicker title key */,
            null /* file chosen */,
            null /* referrer */,
            doc.cookieJarSettings,
            null /* initiating document */,
            false /* don't skip prompt for a location */,
            null /* cache key */,
            PrivateBrowsingUtils.isBrowserPrivate(
              browser
            ) /* private browsing ? */,
            Services.scriptSecurityManager.getSystemPrincipal()
          );
        },
        onError(status) {
          throw new Error("JSON Viewer's onSave failed in startPersistence");
        },
      });
    }
  },
};

var EXPORTED_SYMBOLS = ["DevToolsStartup", "validateProfilerWebChannelUrl"];

// Record Replay stuff.

const { setTimeout, setInterval } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
});

function recordReplayLog(text) {
  dump(`${text}\n`);
}

ChromeUtils.recordReplayLog = recordReplayLog;

function isLoggedIn() {
  const userPref = Services.prefs.getStringPref("devtools.recordreplay.user");
  if (userPref == "") {
    return;
  }
  const user = JSON.parse(userPref);

  // Older versions of Replay did not store the raw object from Auth0 and
  // instead stored backend DB user info, so we need to explicitly look for
  // "sub", not just any parsed object.
  return user == "" ? null : !!user?.sub;
}

async function saveRecordingUser(user) {
  if (!user) {
    Services.prefs.setStringPref("devtools.recordreplay.user", "");
    return;
  }

  Services.prefs.setStringPref(
    "devtools.recordreplay.user",
    JSON.stringify(user)
  );
}

function createRecordingButton() {
  runTestScript();

  let item = {
    id: "record-button",
    type: "button",
    tooltiptext: "record-button.tooltiptext2",
    onClick(evt) {
      if (getConnectionStatus() || !gHasRecordingDriver || isRecordingAllTabs()) {
        return;
      }

      const { gBrowser } = evt.target.ownerDocument.defaultView;
      const recording = gBrowser.selectedBrowser.hasAttribute(
        "recordExecution"
      );

      if (recording) {
        reloadAndStopRecordingTab(gBrowser);
      } else {
        reloadAndRecordTab(gBrowser);
      }
    },
    onCreated(node) {
      function selectedBrowserHasAttribute(attr) {
        try {
          return node.ownerDocument.defaultView.gBrowser.selectedBrowser.hasAttribute(
            attr
          );
        } catch (e) {
          return false;
        }
      }

      node.refreshStatus = () => {
        const recording = selectedBrowserHasAttribute("recordExecution") || isRecordingAllTabs();

        node.classList.toggle("recording", recording);
        node.classList.toggle("hidden", isAuthenticationEnabled() && !isLoggedIn() && !isRunningTest() && !isRecordingAllTabs());

        const status = getConnectionStatus();
        let tooltip = status;
        if (status) {
          node.disabled = true;
        } else if (!gHasRecordingDriver) {
          node.disabled = true;
          tooltip = "missingDriver.label";
        } else if (recording) {
          node.disabled = isRecordingAllTabs();
          tooltip = "stopRecording.label";
        } else {
          node.disabled = false;
          tooltip = "startRecording.label";
        }

        const text = StartupBundle.GetStringFromName(tooltip);
        node.setAttribute("tooltiptext", text);
      };
      node.refreshStatus();

      Services.prefs.addObserver("devtools.recordreplay.user", () => {
        node.refreshStatus();
      });
    },
  };
  CustomizableUI.createWidget(item);
  CustomizableWidgets.push(item);

  item = {
    id: "cloud-recordings-button",
    type: "button",
    tooltiptext: "cloud-recordings-button.tooltiptext2",
    onClick: viewRecordings,
  };
  CustomizableUI.createWidget(item);
  CustomizableWidgets.push(item);

  item = {
    id: "replay-signin-button",
    type: "button",
    tooltiptext: "replay-signin-button.tooltiptext2",
    onClick(evt) {
      const { gBrowser } = evt.target.ownerDocument.defaultView;
      const triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
      Browser.loadURI("https://replay.io/view?forceOpenAuth=true", { triggeringPrincipal });
    },
    onCreated(node) {
      node.refreshStatus = () => {
        node.classList.toggle("hidden", !isAuthenticationEnabled() || isLoggedIn() || isRunningTest() || isRecordingAllTabs());
      };
      node.refreshStatus();

      Services.prefs.addObserver("devtools.recordreplay.user", () => {
        node.refreshStatus();
      });
    },
  };
  CustomizableUI.createWidget(item);
  CustomizableWidgets.push(item);

  setConnectionStatusChangeCallback(status => {
    dump(`CloudReplayStatus ${status}\n`);
    refreshAllRecordingButtons();
  });
}

function refreshAllRecordingButtons() {
  try {
    for (const w of Services.wm.getEnumerator("navigator:browser")) {
      const node = w.document.getElementById("record-button");
      if (node) {
        node.refreshStatus();
      }
    }
  } catch (e) {}
}

// When state changes which affects the recording buttons, we try to update the
// buttons immediately, but make sure that the recording button state does not
// get out of sync with the display state of the button.
setInterval(refreshAllRecordingButtons, 2000);

function isRunningTest() {
  return !!env.get("RECORD_REPLAY_TEST_SCRIPT");
}

async function runTestScript() {
  const script = env.get("RECORD_REPLAY_TEST_SCRIPT");
  if (!script) {
    return;
  }

  // Make sure we have a window.
  while (!Services.wm.getMostRecentWindow("navigator:browser")) {
    dump(`No window for test script, waiting...\n`);
    await new Promise(resolve => setTimeout(resolve, 100));
  }

  const contents = await OS.File.read(script);
  const text = new TextDecoder("utf-8").decode(contents);
  eval(text);
}

function getDispatchServer(url) {
  const address = env.get("RECORD_REPLAY_SERVER");
  if (address) {
    return address;
  }
  return Services.prefs.getStringPref("devtools.recordreplay.cloudServer");
}

function getRecordReplayPlatform() {
  switch (AppConstants.platform) {
    case "macosx":
      return "macOS";
    case "linux":
      return "linux";
    default:
      throw new Error(`Unrecognized platform ${AppConstants.platform}`);
  }
}

function DriverName() {
  return `${getRecordReplayPlatform()}-recordreplay.so`;
}

function DriverJSON() {
  return `${getRecordReplayPlatform()}-recordreplay.json`;
}

function driverFile() {
  const file = Services.dirsvc.get("UAppData", Ci.nsIFile);
  file.append(DriverName());
  return file;
}

function driverJSONFile() {
  const file = Services.dirsvc.get("UAppData", Ci.nsIFile);
  file.append(DriverJSON());
  return file;
}

function crashLogFile() {
  const file = Services.dirsvc.get("UAppData", Ci.nsIFile);
  file.append("crashes.log");
  return file;
}

// Set the crash log file which the driver will use.
env.set("RECORD_REPLAY_CRASH_LOG", crashLogFile().path);

async function fetchURL(url) {
  const response = await fetch(url);
  if (response.status < 200 || response.status >= 300) {
    console.error("Error fetching URL", url, response);
    return null;
  }
  return response;
}

let gHasRecordingDriver;

async function updateRecordingDriver() {
  try {
    fetch;
  } catch (e) {
    dump(`updateRecordingDriver: fetch() not in scope, waiting...\n`);
    setTimeout(updateRecordingDriver, 100);
    return;
  }

  // Don't update the driver if one was specified in the environment.
  if (env.get("RECORD_REPLAY_DRIVER")) {
    gHasRecordingDriver = true;
    return;
  }

  // Set the driver path for use in the recording process.
  env.set("RECORD_REPLAY_DRIVER", driverFile().path);

  let downloadURL;
  try {
    downloadURL = Services.prefs.getStringPref(
      "devtools.recordreplay.driverDownloads"
    );
  } catch (e) {
    downloadURL = "https://replay.io/downloads";
  }

  try {
    const driver = driverFile();
    const json = driverJSONFile();

    dump(`updateRecordingDriver Starting... [Driver ${driver.path}]\n`);

    if (driver.exists() && !gHasRecordingDriver) {
      gHasRecordingDriver = true;
      refreshAllRecordingButtons();
    }

    // If we have already downloaded the driver, redownload the server JSON
    // (much smaller than the driver itself) to see if anything has changed.
    if (json.exists() && driver.exists()) {
      const response = await fetchURL(`${downloadURL}/${DriverJSON()}`);
      if (!response) {
        dump(`updateRecordingDriver JSONFetchFailed\n`);
        return;
      }
      const serverJSON = JSON.parse(await response.text());

      const file = await OS.File.read(json.path);
      const currentJSON = JSON.parse(new TextDecoder("utf-8").decode(file));

      if (serverJSON.version == currentJSON.version) {
        // We've already downloaded the latest driver.
        dump(`updateRecordingDriver AlreadyOnLatestVersion\n`);
        return;
      }
    }

    const jsonResponse = await fetchURL(`${downloadURL}/${DriverJSON()}`);
    if (!jsonResponse) {
      dump(`updateRecordingDriver UpdateNeeded JSONFetchFailed\n`);
      return;
    }
    OS.File.writeAtomic(json.path, await jsonResponse.text());

    const driverResponse = await fetchURL(`${downloadURL}/${DriverName()}`);
    if (!driverResponse) {
      dump(`updateRecordingDriver UpdateNeeded DriverFetchFailed\n`);
      return;
    }
    OS.File.writeAtomic(driver.path, await driverResponse.arrayBuffer(), {
      // Write to a temporary path before renaming the result, so that any
      // recording processes hopefully won't try to load partial binaries
      // (they will keep trying if the load fails, though).
      tmpPath: driver.path + ".tmp",

      // Strip quarantine flag from the downloaded file. Even though this is
      // an update to the browser itself, macOS will still quarantine it and
      // prevent it from being loaded into recording processes.
      noQuarantine: true,
    });

    if (!gHasRecordingDriver) {
      gHasRecordingDriver = true;
      refreshAllRecordingButtons();
    }

    dump(`updateRecordingDriver Updated\n`);
  } catch (e) {
    dump(`updateRecordingDriver Exception ${e}\n`);
  }
}

// We check to see if there is a new recording driver every time the browser
// starts up, and periodically after that.
setTimeout(updateRecordingDriver, 0);
setInterval(updateRecordingDriver, 1000 * 60 * 20);

function reloadAndRecordTab(gBrowser) {
  let url = gBrowser.currentURI.spec;

  // Don't preprocess recordings if we will be submitting them for testing.
  try {
    if (
      Services.prefs.getBoolPref("devtools.recordreplay.submitTestRecordings")
    ) {
      env.set("RECORD_REPLAY_DONT_PROCESS_RECORDINGS", "1");
    }
  } catch (e) {}

  // The recording process uses this env var when printing out the recording ID.
  env.set("RECORD_REPLAY_URL", url);

  let remoteType = E10SUtils.getRemoteTypeForURI(
    url,
    /* aMultiProcess */ true,
    /* aRemoteSubframes */ false,
    /* aPreferredRemoteType */ undefined,
    /* aCurrentUri */ null
  );
  if (
    remoteType != E10SUtils.WEB_REMOTE_TYPE &&
    remoteType != E10SUtils.FILE_REMOTE_TYPE
  ) {
    url = "about:blank";
    remoteType = E10SUtils.WEB_REMOTE_TYPE;
  }

  gBrowser.updateBrowserRemoteness(gBrowser.selectedBrowser, {
    recordExecution: getDispatchServer(url),
    newFrameloader: true,
    remoteType,
  });

  gBrowser.loadURI(url, {
    triggeringPrincipal: gBrowser.selectedBrowser.contentPrincipal,
  });
}

// Return whether all tabs are automatically being recorded.
function isRecordingAllTabs() {
  // Even though this env var indicates we are only interested in recordings
  // for a specific URL, all tabs will be recorded in case they load any
  // matching content.
  return env.get("RECORD_REPLAY_MATCHING_URL");
}

function reloadAndStopRecordingTab(gBrowser) {
  const remoteTab = gBrowser.selectedTab.linkedBrowser.frameLoader.remoteTab;
  if (!remoteTab || !remoteTab.finishRecording()) {
    return;
  }

  recordReplayLog(`WaitForFinishedRecording`);
}

function getBrowserForPid(pid) {
  for (const window of Services.wm.getEnumerator("navigator:browser")) {
    for (const tab of window.gBrowser.tabs) {
      const { remoteTab } = tab.linkedBrowser.frameLoader;
      if (remoteTab && remoteTab.osPid === pid) {
        return tab.linkedBrowser;
      }
    }
  }
  throw new Error("Unable to find browser for recording");
}

function onRecordingStarted(recording) {
  const triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  // There can occasionally be times when the browser isn't found when the
  // recording begins, so we lazily look it up the first time it is needed.
  let browser = null;
  function getBrowser() {
    // If the browser tab is moved to a new window, the cached browser object isn't
    // valid anymore so we need to find the new one.
    if (!browser || !browser.getTabBrowser()) {
      browser = getBrowserForPid(recording.osPid)
    }
    return browser;
  }

  let oldURL;
  let urlLoadOpts;

  function clearRecordingState() {
    getBrowser().getTabBrowser().updateBrowserRemoteness(getBrowser(), {
      recordExecution: undefined,
      newFrameloader: true,
      remoteType: E10SUtils.WEB_REMOTE_TYPE,
    });
  }
  recording.on("unusable", function(name, data) {
    clearRecordingState();

    const { why } = data;
    getBrowser().loadURI(`about:replay?error=${why}`, { triggeringPrincipal });
  });
  recording.on("finished", function() {
    oldURL = getBrowser().currentURI.spec;
    urlLoadOpts = { triggeringPrincipal, oldRecordedURL: oldURL };

    clearRecordingState();

    // The recording has finished, so we need to navigate somewhere or else
    // the user will be shown the tab-crash page while we wait for the recording
    // to finish saving.
    getBrowser().loadURI(`${getViewURL()}?loading=true`, urlLoadOpts);

    recordReplayLog(`WaitForSavedRecording`);
  });
  recording.on("saved", function(name, data) {
    const recordingId = data.id;

    // When the submitTestRecordings pref is set we don't load the viewer,
    // but show a simple page that the recording was submitted, to make things
    // simpler for QA and provide feedback that the pref was set correctly.
    if (
      Services.prefs.getBoolPref("devtools.recordreplay.submitTestRecordings")
    ) {
      fetch(`https://test-inbox.replay.io/${recordingId}:${oldURL}`);
      const why = `Test recording added: ${recordingId}`;
      getBrowser().loadURI(`about:replay?submitted=${why}`, urlLoadOpts);
      return;
    }

    recordReplayLog(`FinishedRecording ${recordingId}`);

    // Find the dispatcher to connect to.
    const dispatchAddress = getDispatchServer();

    let extra = "";

    // Specify the dispatch address if it is not the default.
    if (dispatchAddress != "wss://dispatch.replay.io") {
      extra += `&dispatch=${dispatchAddress}`;
    }

    // For testing, allow specifying a test script to load in the tab.
    const localTest = env.get("RECORD_REPLAY_LOCAL_TEST");
    if (localTest) {
      extra += `&test=${localTest}`;
    } else if (!isAuthenticationEnabled()) {
      // Adding this urlparam disables checks in the devtools that the user has
      // permission to view the recording.
      extra += `&test=1`;
    }

    getBrowser().loadURI(`${getViewURL()}?id=${recordingId}${extra}`, urlLoadOpts);
  });
}

Services.obs.addObserver(
  subject => onRecordingStarted(subject.wrappedJSObject),
  "recordreplay-recording-started"
);

function getViewURL() {
  let viewHost = "https://replay.io";

  // For testing, allow overriding the host for the view page.
  const hostOverride = env.get("RECORD_REPLAY_VIEW_HOST");
  if (hostOverride) {
    viewHost = hostOverride;
  }
  return `${viewHost}/view`;
}

function viewRecordings(evt) {
  const { gBrowser } = evt.target.ownerDocument.defaultView;
  const triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  gBrowser.loadURI(
    Services.prefs.getStringPref("devtools.recordreplay.recordingsUrl"),
    { triggeringPrincipal }
  );
}

function isAuthenticationEnabled() {
  // Authentication is controlled by a preference but can be disabled by an
  // environment variable.
  return (
    Services.prefs.getBoolPref(
      "devtools.recordreplay.authentication-enabled"
    ) && !env.get("RECORD_REPLAY_DISABLE_AUTHENTICATION")
  );
}
