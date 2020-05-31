/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Logic that always runs in recording/replaying processes, and which can affect the recording.

// Create a sandbox with resources from Gecko components we need.
const sandbox = Cu.Sandbox(
  Components.Constructor("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
  {
    wantGlobalProperties: ["InspectorUtils", "CSSRule"],
  }
);
Cu.evalInSandbox(
  "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
  "Components.utils.import('resource://gre/modules/Services.jsm');" +
  "addDebuggerToGlobal(this);",
  sandbox
);
const {
  Debugger,
  RecordReplayControl,
  Services,
  InspectorUtils,
  CSSRule,
  findClosestPoint,
} = sandbox;

// This script can be loaded into no-recording/replaying processes during automated tests.
const isRecordingOrReplaying = !!RecordReplayControl.progressCounter;

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

let gWindow;
function getWindow() {
  if (!gWindow) {
    for (const w of Services.ww.getWindowEnumerator()) {
      gWindow = w;
      break;
    }
  }
  return gWindow;
}

const gDebugger = new Debugger();
const gSandboxGlobal = gDebugger.makeGlobalObjectReference(sandbox);
const gAllGlobals = [];

function considerScript(script) {
  return RecordReplayControl.shouldUpdateProgressCounter(script.url);
}

function countScriptFrames() {
  let count = 0;
  for (let frame = gDebugger.getNewestFrame(); frame; frame = frame.older) {
    if (considerScript(frame.script)) {
      count++;
    }
  }
  return count;
}

function CanCreateCheckpoint() {
  return countScriptFrames() == 0;
}

const gNewGlobalHooks = [];
gDebugger.onNewGlobalObject = global => {
  try {
    gDebugger.addDebuggee(global);
    gAllGlobals.push(global);
    gNewGlobalHooks.forEach(hook => hook(global));
  } catch (e) {}
};

// The UI process must wait until the content global is created here before
// URLs can be loaded.
Services.obs.addObserver(
  { observe: () => Services.cpmm.sendAsyncMessage("RecordingInitialized") },
  "content-document-global-created"
);

function IdMap() {
  this._idMap = [undefined];
  this._objectMap = new Map();
}

IdMap.prototype = {
  add(obj) {
    if (this._objectMap.has(obj)) {
      return this._objectMap.get(obj);
    }
    const id = this._idMap.length;
    this._idMap.push(obj);
    this._objectMap.set(obj, id);
    return id;
  },

  getId(obj) {
    return this._objectMap.get(obj) || 0;
  },

  getObject(id) {
    return this._idMap[id];
  },

  map(callback) {
    const rv = [];
    for (let i = 1; i < this._idMap.length; i++) {
      rv.push(callback(i));
    }
    return rv;
  },

  forEach(callback) {
    for (let i = 1; i < this._idMap.length; i++) {
      callback(i, this._idMap[i]);
    }
  },
};

const gScripts = new IdMap();
const gSources = new Set();

gDebugger.onNewScript = script => {
  if (!isRecordingOrReplaying || RecordReplayControl.areThreadEventsDisallowed()) {
    return;
  }

  if (!considerScript(script)) {
    ignoreScript(script);
    return;
  }

  addScript(script);

  if (!gSources.has(script.source)) {
    gSources.add(script.source);
    if (script.source.sourceMapURL &&
        Services.prefs.getBoolPref("devtools.recordreplay.uploadSourceMaps")) {
      const pid = RecordReplayControl.middlemanPid();
      const { url, text, sourceMapURL } = script.source;
      Services.cpmm.sendAsyncMessage(
        "RecordReplayGeneratedSourceWithSourceMap",
        { pid, url, text, sourceMapURL }
      );
    }
  }

  if (exports.OnNewScript) {
    exports.OnNewScript(script);
  }

  function addScript(script) {
    const id = gScripts.add(script);
    script.setInstrumentationId(id);
    script.getChildScripts().forEach(addScript);
  }

  function ignoreScript(script) {
    script.setInstrumentationId(0);
    script.getChildScripts().forEach(ignoreScript);
  }
};

getWindow().docShell.watchedByDevtools = true;
Services.obs.addObserver(
  {
    observe(subject) {
      subject.QueryInterface(Ci.nsIDocShell);
      subject.watchedByDevtools = true;
    },
  },
  "webnavigation-create"
);

Services.obs.addObserver(
  {
    observe(_1, _2, data) {
      if (exports.OnHTMLContent) {
        exports.OnHTMLContent(data);
      }
    },
  },
  "devtools-html-content"
);

Services.console.registerListener({
  observe(message) {
    if (!(message instanceof Ci.nsIScriptError)) {
      return;
    }

    advanceProgressCounter();

    if (exports.OnConsoleError) {
      exports.OnConsoleError(message);
    }
  }
});

Services.obs.addObserver({
  observe(message) {
    if (exports.OnConsoleAPICall) {
      exports.OnConsoleAPICall(message);
    }
  },
}, "console-api-log-event");

getWindow().docShell.chromeEventHandler.addEventListener(
  "DOMWindowCreated",
  () => {
    const window = getWindow();

    window.document.styleSheetChangeEventsEnabled = true;

    if (exports.OnWindowCreated) {
      exports.OnWindowCreated(window);
    }
  },
  true
);

getWindow().docShell.chromeEventHandler.addEventListener(
  "StyleSheetApplicableStateChanged",
  ({ stylesheet }) => {
    if (exports.OnStyleSheetChange) {
      exports.OnStyleSheetChange(stylesheet);
    }
  },
  true
);

function advanceProgressCounter() {
  if (!isRecordingOrReplaying) {
    return;
  }
  let progress = RecordReplayControl.progressCounter();
  RecordReplayControl.setProgressCounter(++progress);
  return progress;
}

function OnMouseEvent(time, kind, x, y) {
  advanceProgressCounter();
};

const { DebuggerNotificationObserver } = Cu.getGlobalForObject(require("resource://devtools/shared/Loader.jsm"));
const gNotificationObserver = new DebuggerNotificationObserver();
gNotificationObserver.addListener(eventListener);
gNewGlobalHooks.push(global => {
  try {
    gNotificationObserver.connect(global.unsafeDereference());
  } catch (e) {}
});

const { eventBreakpointForNotification } = require("devtools/server/actors/utils/event-breakpoints");

function eventListener(info) {
  const event = eventBreakpointForNotification(gDebugger, info);
  if (!event) {
    return;
  }
  advanceProgressCounter();

  if (exports.OnEvent) {
    exports.OnEvent(info.phase, event);
  }
}

function SendRecordingData(pid, offset, length, buf, totalLength, duration) {
  let description;
  if (totalLength) {
    // Supply a description for this recording, it is about to be finished.
    const data = RecordReplayControl.getGraphics(
      /* repaint */ false,
      "image/jpeg",
      "quality=50"
    );
    // Convert seconds to milliseconds
    duration = (duration * 1000) | 0;
    description = {
      length: totalLength,
      duration,
      lastScreenMimeType: "image/jpeg",
      lastScreenData: data,
    };
  }
  Services.cpmm.sendAsyncMessage("UploadRecordingData",
                                 { pid, offset, length, buf, description });
}

function OnTestCommand(str) {
  const [_, cmd, arg] = /(.*?) (.*)/.exec(str);
  switch (cmd) {
    case "WebReplaySendAsyncMessage":
      Services.cpmm.sendAsyncMessage(arg);
      break;
    default:
      dump(`Unrecognized Test Command ${cmd}\n`);
      break;
  }
}

const exports = {
  CanCreateCheckpoint,
  OnMouseEvent,
  SendRecordingData,
  OnTestCommand,
};

function Initialize(text) {
  try {
    if (text) {
      const imports = {
        Cc, Ci, Cu, ChromeUtils, Debugger, RecordReplayControl, InspectorUtils,
        considerScript, countScriptFrames, gScripts, gDebugger,
        advanceProgressCounter, gAllGlobals, getWindow, gSandboxGlobal,
        gNewGlobalHooks, Services,
      };
      Object.assign(exports, new Function("imports", `${text} return exports`)(imports));
    }
    return exports;
  } catch (e) {
    dump(`Initialize Error: ${e}\n`);
  }
}

var EXPORTED_SYMBOLS = ["Initialize"];
