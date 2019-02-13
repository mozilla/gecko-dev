/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

const { Services } = require("resource://gre/modules/Services.jsm");

loader.lazyGetter(this, "osString", () => Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS);

// Panels
loader.lazyGetter(this, "OptionsPanel", () => require("devtools/framework/toolbox-options").OptionsPanel);
loader.lazyGetter(this, "InspectorPanel", () => require("devtools/inspector/inspector-panel").InspectorPanel);
loader.lazyGetter(this, "WebConsolePanel", () => require("devtools/webconsole/panel").WebConsolePanel);
loader.lazyGetter(this, "DebuggerPanel", () => require("devtools/debugger/panel").DebuggerPanel);
loader.lazyGetter(this, "StyleEditorPanel", () => require("devtools/styleeditor/styleeditor-panel").StyleEditorPanel);
loader.lazyGetter(this, "ShaderEditorPanel", () => require("devtools/shadereditor/panel").ShaderEditorPanel);
loader.lazyGetter(this, "CanvasDebuggerPanel", () => require("devtools/canvasdebugger/panel").CanvasDebuggerPanel);
loader.lazyGetter(this, "WebAudioEditorPanel", () => require("devtools/webaudioeditor/panel").WebAudioEditorPanel);
loader.lazyGetter(this, "PerformancePanel", () => require("devtools/performance/panel").PerformancePanel);
loader.lazyGetter(this, "NetMonitorPanel", () => require("devtools/netmonitor/panel").NetMonitorPanel);
loader.lazyGetter(this, "StoragePanel", () => require("devtools/storage/panel").StoragePanel);
loader.lazyGetter(this, "ScratchpadPanel", () => require("devtools/scratchpad/scratchpad-panel").ScratchpadPanel);

// Strings
const toolboxProps = "chrome://browser/locale/devtools/toolbox.properties";
const inspectorProps = "chrome://browser/locale/devtools/inspector.properties";
const webConsoleProps = "chrome://browser/locale/devtools/webconsole.properties";
const debuggerProps = "chrome://browser/locale/devtools/debugger.properties";
const styleEditorProps = "chrome://browser/locale/devtools/styleeditor.properties";
const shaderEditorProps = "chrome://browser/locale/devtools/shadereditor.properties";
const canvasDebuggerProps = "chrome://browser/locale/devtools/canvasdebugger.properties";
const webAudioEditorProps = "chrome://browser/locale/devtools/webaudioeditor.properties";
const profilerProps = "chrome://browser/locale/devtools/profiler.properties";
const netMonitorProps = "chrome://browser/locale/devtools/netmonitor.properties";
const storageProps = "chrome://browser/locale/devtools/storage.properties";
const scratchpadProps = "chrome://browser/locale/devtools/scratchpad.properties";

loader.lazyGetter(this, "toolboxStrings", () => Services.strings.createBundle(toolboxProps));
loader.lazyGetter(this, "profilerStrings",() => Services.strings.createBundle(profilerProps));
loader.lazyGetter(this, "webConsoleStrings", () => Services.strings.createBundle(webConsoleProps));
loader.lazyGetter(this, "debuggerStrings", () => Services.strings.createBundle(debuggerProps));
loader.lazyGetter(this, "styleEditorStrings", () => Services.strings.createBundle(styleEditorProps));
loader.lazyGetter(this, "shaderEditorStrings", () => Services.strings.createBundle(shaderEditorProps));
loader.lazyGetter(this, "canvasDebuggerStrings", () => Services.strings.createBundle(canvasDebuggerProps));
loader.lazyGetter(this, "webAudioEditorStrings", () => Services.strings.createBundle(webAudioEditorProps));
loader.lazyGetter(this, "inspectorStrings", () => Services.strings.createBundle(inspectorProps));
loader.lazyGetter(this, "netMonitorStrings", () => Services.strings.createBundle(netMonitorProps));
loader.lazyGetter(this, "storageStrings", () => Services.strings.createBundle(storageProps));
loader.lazyGetter(this, "scratchpadStrings", () => Services.strings.createBundle(scratchpadProps));

let Tools = {};
exports.Tools = Tools;

// Definitions
Tools.options = {
  id: "options",
  ordinal: 0,
  url: "chrome://browser/content/devtools/framework/toolbox-options.xul",
  icon: "chrome://browser/skin/devtools/tool-options.svg",
  invertIconForLightTheme: true,
  bgTheme: "theme-body",
  label: l10n("options.label", toolboxStrings),
  iconOnly: true,
  panelLabel: l10n("options.panelLabel", toolboxStrings),
  tooltip: l10n("optionsButton.tooltip", toolboxStrings),
  inMenu: false,

  isTargetSupported: function(target) {
    return true;
  },

  build: function(iframeWindow, toolbox) {
    return new OptionsPanel(iframeWindow, toolbox);
  }
}

Tools.inspector = {
  id: "inspector",
  accesskey: l10n("inspector.accesskey", inspectorStrings),
  key: l10n("inspector.commandkey", inspectorStrings),
  ordinal: 1,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  icon: "chrome://browser/skin/devtools/tool-inspector.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/inspector/inspector.xul",
  label: l10n("inspector.label", inspectorStrings),
  panelLabel: l10n("inspector.panelLabel", inspectorStrings),
  get tooltip() {
    return l10n("inspector.tooltip2", inspectorStrings,
    ( osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+" ) + this.key);
  },
  inMenu: true,
  commands: [
    "devtools/resize-commands",
    "devtools/inspector/inspector-commands",
    "devtools/eyedropper/commands.js"
  ],

  preventClosingOnKey: true,
  onkey: function(panel) {
    panel.toolbox.highlighterUtils.togglePicker();
  },

  isTargetSupported: function(target) {
    return target.hasActor("inspector");
  },

  build: function(iframeWindow, toolbox) {
    return new InspectorPanel(iframeWindow, toolbox);
  }
};

Tools.webConsole = {
  id: "webconsole",
  key: l10n("cmd.commandkey", webConsoleStrings),
  accesskey: l10n("webConsoleCmd.accesskey", webConsoleStrings),
  modifiers: Services.appinfo.OS == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 2,
  icon: "chrome://browser/skin/devtools/tool-webconsole.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/webconsole.xul",
  label: l10n("ToolboxTabWebconsole.label", webConsoleStrings),
  menuLabel: l10n("MenuWebconsole.label", webConsoleStrings),
  panelLabel: l10n("ToolboxWebConsole.panelLabel", webConsoleStrings),
  get tooltip() {
    return l10n("ToolboxWebconsole.tooltip2", webConsoleStrings,
    ( osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+" ) + this.key);
  },
  inMenu: true,
  commands: "devtools/webconsole/console-commands",

  preventClosingOnKey: true,
  onkey: function(panel, toolbox) {
    if (toolbox.splitConsole)
      return toolbox.focusConsoleInput();

    panel.focusInput();
  },

  isTargetSupported: function(target) {
    return true;
  },

  build: function(iframeWindow, toolbox) {
    return new WebConsolePanel(iframeWindow, toolbox);
  }
};

Tools.jsdebugger = {
  id: "jsdebugger",
  key: l10n("debuggerMenu.commandkey", debuggerStrings),
  accesskey: l10n("debuggerMenu.accesskey", debuggerStrings),
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 3,
  icon: "chrome://browser/skin/devtools/tool-debugger.svg",
  invertIconForLightTheme: true,
  highlightedicon: "chrome://browser/skin/devtools/tool-debugger-paused.svg",
  url: "chrome://browser/content/devtools/debugger.xul",
  label: l10n("ToolboxDebugger.label", debuggerStrings),
  panelLabel: l10n("ToolboxDebugger.panelLabel", debuggerStrings),
  get tooltip() {
    return l10n("ToolboxDebugger.tooltip2", debuggerStrings,
    ( osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+" ) + this.key);
  },
  inMenu: true,
  commands: "devtools/debugger/debugger-commands",

  isTargetSupported: function(target) {
    return true;
  },

  build: function(iframeWindow, toolbox) {
    return new DebuggerPanel(iframeWindow, toolbox);
  }
};

Tools.styleEditor = {
  id: "styleeditor",
  key: l10n("open.commandkey", styleEditorStrings),
  ordinal: 4,
  accesskey: l10n("open.accesskey", styleEditorStrings),
  modifiers: "shift",
  icon: "chrome://browser/skin/devtools/tool-styleeditor.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/styleeditor.xul",
  label: l10n("ToolboxStyleEditor.label", styleEditorStrings),
  panelLabel: l10n("ToolboxStyleEditor.panelLabel", styleEditorStrings),
  get tooltip() {
    return l10n("ToolboxStyleEditor.tooltip3", styleEditorStrings,
    "Shift+" + functionkey(this.key));
  },
  inMenu: true,
  commands: "devtools/styleeditor/styleeditor-commands",

  isTargetSupported: function(target) {
    return target.hasActor("styleEditor") || target.hasActor("styleSheets");
  },

  build: function(iframeWindow, toolbox) {
    return new StyleEditorPanel(iframeWindow, toolbox);
  }
};

Tools.shaderEditor = {
  id: "shadereditor",
  ordinal: 5,
  visibilityswitch: "devtools.shadereditor.enabled",
  icon: "chrome://browser/skin/devtools/tool-shadereditor.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/shadereditor.xul",
  label: l10n("ToolboxShaderEditor.label", shaderEditorStrings),
  panelLabel: l10n("ToolboxShaderEditor.panelLabel", shaderEditorStrings),
  tooltip: l10n("ToolboxShaderEditor.tooltip", shaderEditorStrings),

  isTargetSupported: function(target) {
    return target.hasActor("webgl") && !target.chrome;
  },

  build: function(iframeWindow, toolbox) {
    return new ShaderEditorPanel(iframeWindow, toolbox);
  }
};

Tools.canvasDebugger = {
  id: "canvasdebugger",
  ordinal: 6,
  visibilityswitch: "devtools.canvasdebugger.enabled",
  icon: "chrome://browser/skin/devtools/tool-styleeditor.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/canvasdebugger.xul",
  label: l10n("ToolboxCanvasDebugger.label", canvasDebuggerStrings),
  panelLabel: l10n("ToolboxCanvasDebugger.panelLabel", canvasDebuggerStrings),
  tooltip: l10n("ToolboxCanvasDebugger.tooltip", canvasDebuggerStrings),

  // Hide the Canvas Debugger in the Add-on Debugger and Browser Toolbox
  // (bug 1047520).
  isTargetSupported: function(target) {
    return target.hasActor("canvas") && !target.chrome;
  },

  build: function (iframeWindow, toolbox) {
    return new CanvasDebuggerPanel(iframeWindow, toolbox);
  }
};

Tools.performance = {
  id: "performance",
  ordinal: 7,
  icon: "chrome://browser/skin/devtools/tool-profiler.svg",
  invertIconForLightTheme: true,
  highlightedicon: "chrome://browser/skin/devtools/tool-profiler-active.svg",
  url: "chrome://browser/content/devtools/performance.xul",
  visibilityswitch: "devtools.performance.enabled",
  label: l10n("profiler.label2", profilerStrings),
  panelLabel: l10n("profiler.panelLabel2", profilerStrings),
  get tooltip() {
    return l10n("profiler.tooltip3", profilerStrings,
    "Shift+" + functionkey(this.key));
  },
  accesskey: l10n("profiler.accesskey", profilerStrings),
  key: l10n("profiler.commandkey2", profilerStrings),
  modifiers: "shift",
  inMenu: true,

  isTargetSupported: function (target) {
    return target.hasActor("profiler");
  },

  build: function (frame, target) {
    return new PerformancePanel(frame, target);
  }
};

Tools.netMonitor = {
  id: "netmonitor",
  accesskey: l10n("netmonitor.accesskey", netMonitorStrings),
  key: l10n("netmonitor.commandkey", netMonitorStrings),
  ordinal: 9,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  visibilityswitch: "devtools.netmonitor.enabled",
  icon: "chrome://browser/skin/devtools/tool-network.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/netmonitor.xul",
  label: l10n("netmonitor.label", netMonitorStrings),
  panelLabel: l10n("netmonitor.panelLabel", netMonitorStrings),
  get tooltip() {
    return l10n("netmonitor.tooltip2", netMonitorStrings,
    ( osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+" ) + this.key);
  },
  inMenu: true,

  isTargetSupported: function(target) {
    return target.getTrait("networkMonitor");
  },

  build: function(iframeWindow, toolbox) {
    return new NetMonitorPanel(iframeWindow, toolbox);
  }
};

Tools.storage = {
  id: "storage",
  key: l10n("storage.commandkey", storageStrings),
  ordinal: 10,
  accesskey: l10n("storage.accesskey", storageStrings),
  modifiers: "shift",
  visibilityswitch: "devtools.storage.enabled",
  icon: "chrome://browser/skin/devtools/tool-storage.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/storage.xul",
  label: l10n("storage.label", storageStrings),
  menuLabel: l10n("storage.menuLabel", storageStrings),
  panelLabel: l10n("storage.panelLabel", storageStrings),
  get tooltip() {
    return l10n("storage.tooltip3", storageStrings,
    "Shift+" + functionkey(this.key));
  },
  inMenu: true,

  isTargetSupported: function(target) {
    return target.isLocalTab ||
           ( target.hasActor("storage") &&
             target.getTrait("storageInspector") );
  },

  build: function(iframeWindow, toolbox) {
    return new StoragePanel(iframeWindow, toolbox);
  }
};

Tools.webAudioEditor = {
  id: "webaudioeditor",
  ordinal: 11,
  visibilityswitch: "devtools.webaudioeditor.enabled",
  icon: "chrome://browser/skin/devtools/tool-webaudio.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/webaudioeditor.xul",
  label: l10n("ToolboxWebAudioEditor1.label", webAudioEditorStrings),
  panelLabel: l10n("ToolboxWebAudioEditor1.panelLabel", webAudioEditorStrings),
  tooltip: l10n("ToolboxWebAudioEditor1.tooltip", webAudioEditorStrings),

  isTargetSupported: function(target) {
    return !target.chrome && target.hasActor("webaudio");
  },

  build: function(iframeWindow, toolbox) {
    return new WebAudioEditorPanel(iframeWindow, toolbox);
  }
};

Tools.scratchpad = {
  id: "scratchpad",
  ordinal: 12,
  visibilityswitch: "devtools.scratchpad.enabled",
  icon: "chrome://browser/skin/devtools/tool-scratchpad.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/scratchpad.xul",
  label: l10n("scratchpad.label", scratchpadStrings),
  panelLabel: l10n("scratchpad.panelLabel", scratchpadStrings),
  tooltip: l10n("scratchpad.tooltip", scratchpadStrings),
  inMenu: false,
  commands: "devtools/scratchpad/scratchpad-commands",

  isTargetSupported: function(target) {
    return target.isRemote;
  },

  build: function(iframeWindow, toolbox) {
    return new ScratchpadPanel(iframeWindow, toolbox);
  }
};

let defaultTools = [
  Tools.options,
  Tools.webConsole,
  Tools.inspector,
  Tools.jsdebugger,
  Tools.styleEditor,
  Tools.shaderEditor,
  Tools.canvasDebugger,
  Tools.webAudioEditor,
  Tools.performance,
  Tools.netMonitor,
  Tools.storage,
  Tools.scratchpad
];

exports.defaultTools = defaultTools;

Tools.darkTheme = {
  id: "dark",
  label: l10n("options.darkTheme.label", toolboxStrings),
  ordinal: 1,
  stylesheets: ["chrome://browser/skin/devtools/dark-theme.css"],
  classList: ["theme-dark"],
};

Tools.lightTheme = {
  id: "light",
  label: l10n("options.lightTheme.label", toolboxStrings),
  ordinal: 2,
  stylesheets: ["chrome://browser/skin/devtools/light-theme.css"],
  classList: ["theme-light"],
};

exports.defaultThemes = [
  Tools.darkTheme,
  Tools.lightTheme,
];

/**
 * Lookup l10n string from a string bundle.
 *
 * @param {string} name
 *        The key to lookup.
 * @param {StringBundle} bundle
 *        The key to lookup.
 * @returns A localized version of the given key.
 */
function l10n(name, bundle, arg)
{
  try {
    return arg ? bundle.formatStringFromName(name, [arg], 1)
    : bundle.GetStringFromName(name);
  } catch (ex) {
    Services.console.logStringMessage("Error reading '" + name + "'");
    throw new Error("l10n error with " + name);
  }
}

function functionkey(shortkey)
{
  return shortkey.split("_")[1];
}
