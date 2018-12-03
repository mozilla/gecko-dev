/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { loader, require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});

// Require this module to setup core modules
loader.require("devtools/client/framework/devtools-browser");

var { gDevTools } = require("devtools/client/framework/devtools");
var { TargetFactory } = require("devtools/client/framework/target");
var { Toolbox } = require("devtools/client/framework/toolbox");
var Services = require("Services");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");
var { PrefsHelper } = require("devtools/client/shared/prefs");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");
const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

// Timeout to wait before we assume that a connect() timed out without an error.
// In milliseconds. (With the Debugger pane open, this has been reported to last
// more than 10 seconds!)
const STATUS_REVEAL_TIME = 15000;

/**
 * Shortcuts for accessing various debugger preferences.
 */
var Prefs = new PrefsHelper("devtools.debugger", {
  chromeDebuggingHost: ["Char", "chrome-debugging-host"],
  chromeDebuggingWebSocket: ["Bool", "chrome-debugging-websocket"],
});

var gToolbox, gClient, gShortcuts;

function appendStatusMessage(msg) {
  const statusMessage = document.getElementById("status-message");
  statusMessage.textContent += msg + "\n";
  if (msg.stack) {
    statusMessage.textContent += msg.stack + "\n";
  }
}

function toggleStatusMessage(visible = true) {
  const statusMessageContainer = document.getElementById("status-message-container");
  if (visible) {
    statusMessageContainer.removeAttribute("hidden");
  } else {
    statusMessageContainer.setAttribute("hidden", "true");
  }
}

function revealStatusMessage() {
  toggleStatusMessage(true);
}

function hideStatusMessage() {
  toggleStatusMessage(false);
}

var connect = async function() {
  // Initiate the connection
  const env = Cc["@mozilla.org/process/environment;1"]
    .getService(Ci.nsIEnvironment);
  const port = env.get("MOZ_BROWSER_TOOLBOX_PORT");
  const addonID = env.get("MOZ_BROWSER_TOOLBOX_ADDONID");

  // A port needs to be passed in from the environment, for instance:
  //    MOZ_BROWSER_TOOLBOX_PORT=6080 ./mach run -chrome \
  //      chrome://devtools/content/framework/toolbox-process-window.html
  if (!port) {
    throw new Error("Must pass a port in an env variable with MOZ_BROWSER_TOOLBOX_PORT");
  }

  const host = Prefs.chromeDebuggingHost;
  const webSocket = Prefs.chromeDebuggingWebSocket;
  appendStatusMessage(`Connecting to ${host}:${port}, ws: ${webSocket}`);
  const transport = await DebuggerClient.socketConnect({
    host,
    port,
    webSocket,
  });
  gClient = new DebuggerClient(transport);
  appendStatusMessage("Start protocol client for connection");
  await gClient.connect();

  appendStatusMessage("Get root form for toolbox");
  if (addonID) {
    const addonTargetFront = await gClient.mainRoot.getAddon({ id: addonID });
    await openToolbox({activeTab: addonTargetFront, chrome: true});
  } else {
    const front = await gClient.mainRoot.getMainProcess();
    await openToolbox({activeTab: front, chrome: true});
  }
};

// Certain options should be toggled since we can assume chrome debugging here
function setPrefDefaults() {
  Services.prefs.setBoolPref("devtools.inspector.showUserAgentStyles", true);
  Services.prefs.setBoolPref("devtools.performance.ui.show-platform-data", true);
  Services.prefs.setBoolPref("devtools.inspector.showAllAnonymousContent", true);
  Services.prefs.setBoolPref("browser.dom.window.dump.enabled", true);
  Services.prefs.setBoolPref("devtools.console.stdout.chrome", true);
  Services.prefs.setBoolPref("devtools.command-button-noautohide.enabled", true);
  // Bug 1225160 - Using source maps with browser debugging can lead to a crash
  Services.prefs.setBoolPref("devtools.debugger.source-maps-enabled", false);
  Services.prefs.setBoolPref("devtools.preference.new-panel-enabled", false);
  Services.prefs.setBoolPref("layout.css.emulate-moz-box-with-flex", false);

  Services.prefs.setBoolPref("devtools.performance.enabled", false);
}

window.addEventListener("load", async function() {
  gShortcuts = new KeyShortcuts({window});
  gShortcuts.on("CmdOrCtrl+W", onCloseCommand);

  const statusMessageContainer = document.getElementById("status-message-title");
  statusMessageContainer.textContent = L10N.getStr("browserToolbox.statusMessage");

  setPrefDefaults();

  // Reveal status message if connecting is slow or if an error occurs.
  const delayedStatusReveal = setTimeout(revealStatusMessage, STATUS_REVEAL_TIME);
  try {
    await connect();
    clearTimeout(delayedStatusReveal);
    hideStatusMessage();
  } catch (e) {
    clearTimeout(delayedStatusReveal);
    appendStatusMessage(e);
    revealStatusMessage();
    console.error(e);
  }
}, { once: true });

function onCloseCommand(event) {
  window.close();
}

async function openToolbox({ form, activeTab, chrome }) {
  let options = {
    form,
    activeTab,
    client: gClient,
    chrome,
  };
  appendStatusMessage(`Create toolbox target: ${JSON.stringify({form, chrome}, null, 2)}`);
  const target = await TargetFactory.forRemoteTab(options);
  const frame = document.getElementById("toolbox-iframe");

  // Remember the last panel that was used inside of this profile.
  // But if we are testing, then it should always open the debugger panel.
  const selectedTool =
    Services.prefs.getCharPref("devtools.browsertoolbox.panel",
      Services.prefs.getCharPref("devtools.toolbox.selectedTool",
                                  "jsdebugger"));

  options = { customIframe: frame };
  appendStatusMessage(`Show toolbox with ${selectedTool} selected`);
  const toolbox = await gDevTools.showToolbox(
    target,
    selectedTool,
    Toolbox.HostType.CUSTOM,
    options
  );
  onNewToolbox(toolbox);
}

function onNewToolbox(toolbox) {
  gToolbox = toolbox;
  bindToolboxHandlers();
  raise();
  const env = Cc["@mozilla.org/process/environment;1"]
    .getService(Ci.nsIEnvironment);
  const testScript = env.get("MOZ_TOOLBOX_TEST_SCRIPT");
  if (testScript) {
    // Only allow executing random chrome scripts when a special
    // test-only pref is set
    const prefName = "devtools.browser-toolbox.allow-unsafe-script";
    if (Services.prefs.getPrefType(prefName) == Services.prefs.PREF_BOOL &&
        Services.prefs.getBoolPref(prefName) === true) {
      evaluateTestScript(testScript, toolbox);
    }
  }
}

function evaluateTestScript(script, toolbox) {
  const sandbox = Cu.Sandbox(window);
  sandbox.window = window;
  sandbox.toolbox = toolbox;
  sandbox.ChromeUtils = ChromeUtils;
  Cu.evalInSandbox(script, sandbox);
}

async function bindToolboxHandlers() {
  gToolbox.once("destroyed", quitApp);
  window.addEventListener("unload", onUnload);

  if (Services.appinfo.OS == "Darwin") {
    // Badge the dock icon to differentiate this process from the main application
    // process.
    updateBadgeText(false);

    // Once the debugger panel opens listen for thread pause / resume.
    const panel = await gToolbox.getPanelWhenReady("jsdebugger");
    setupThreadListeners(panel);
  }
}

function setupThreadListeners(panel) {
  updateBadgeText(panel.isPaused());

  const onPaused = updateBadgeText.bind(null, true);
  const onResumed = updateBadgeText.bind(null, false);
  gToolbox.target.on("thread-paused", onPaused);
  gToolbox.target.on("thread-resumed", onResumed);

  panel.once("destroyed", () => {
    gToolbox.target.off("thread-paused", onPaused);
    gToolbox.target.off("thread-resumed", onResumed);
  });
}

function updateBadgeText(paused) {
  const dockSupport = Cc["@mozilla.org/widget/macdocksupport;1"]
    .getService(Ci.nsIMacDockSupport);
  dockSupport.badgeText = paused ? "▐▐ " : " ▶";
}

function onUnload() {
  window.removeEventListener("unload", onUnload);
  window.removeEventListener("message", onMessage);
  gToolbox.destroy();
}

function onMessage(event) {
  if (!event.data) {
    return;
  }
  const msg = event.data;
  switch (msg.name) {
    case "toolbox-raise":
      raise();
      break;
    case "toolbox-title":
      setTitle(msg.data.value);
      break;
  }
}

window.addEventListener("message", onMessage);

function raise() {
  window.focus();
}

function setTitle(title) {
  document.title = title;
}

function quitApp() {
  const quit = Cc["@mozilla.org/supports-PRBool;1"]
             .createInstance(Ci.nsISupportsPRBool);
  Services.obs.notifyObservers(quit, "quit-application-requested");

  const shouldProceed = !quit.data;
  if (shouldProceed) {
    Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
  }
}
