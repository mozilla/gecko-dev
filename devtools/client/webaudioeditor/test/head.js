/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);

var { DebuggerServer } = require("devtools/server/main");
var { generateUUID } = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

var { WebAudioFront } = require("devtools/shared/fronts/webaudio");
var audioNodes = require("devtools/server/actors/utils/audionodes.json");

const EXAMPLE_URL = "http://example.com/browser/devtools/client/webaudioeditor/test/";
const SIMPLE_CONTEXT_URL = EXAMPLE_URL + "doc_simple-context.html";
const COMPLEX_CONTEXT_URL = EXAMPLE_URL + "doc_complex-context.html";
const SIMPLE_NODES_URL = EXAMPLE_URL + "doc_simple-node-creation.html";
const MEDIA_NODES_URL = EXAMPLE_URL + "doc_media-node-creation.html";
const BUFFER_AND_ARRAY_URL = EXAMPLE_URL + "doc_buffer-and-array.html";
const DESTROY_NODES_URL = EXAMPLE_URL + "doc_destroy-nodes.html";
const CONNECT_PARAM_URL = EXAMPLE_URL + "doc_connect-param.html";
const CONNECT_MULTI_PARAM_URL = EXAMPLE_URL + "doc_connect-multi-param.html";
const IFRAME_CONTEXT_URL = EXAMPLE_URL + "doc_iframe-context.html";
const AUTOMATION_URL = EXAMPLE_URL + "doc_automation.html";

// Enable logging for all the tests. Both the debugger server and frontend will
// be affected by this pref.
var gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
Services.prefs.setBoolPref("devtools.debugger.log", false);

var gToolEnabled = Services.prefs.getBoolPref("devtools.webaudioeditor.enabled");

registerCleanupFunction(() => {
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
  Services.prefs.setBoolPref("devtools.webaudioeditor.enabled", gToolEnabled);
  Cu.forceGC();
});

function reload(aTarget, aWaitForTargetEvent = "navigate") {
  aTarget.activeTab.reload();
  return once(aTarget, aWaitForTargetEvent);
}

function navigate(aTarget, aUrl, aWaitForTargetEvent = "navigate") {
  executeSoon(() => aTarget.activeTab.navigateTo({ url: aUrl }));
  return once(aTarget, aWaitForTargetEvent);
}

/**
 * Adds a new tab, and instantiate a WebAudiFront object.
 * This requires calling removeTab before the test ends.
 */
function initBackend(aUrl) {
  info("Initializing a web audio editor front.");

  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  return (async function() {
    const tab = await addTab(aUrl);
    const target = await TargetFactory.forTab(tab);

    await target.attach();

    const front = new WebAudioFront(target.client, target.form);
    return { target, front };
  })();
}

/**
 * Adds a new tab, and open the toolbox for that tab, selecting the audio editor
 * panel.
 * This requires calling teardown before the test ends.
 */
function initWebAudioEditor(aUrl) {
  info("Initializing a web audio editor pane.");

  return (async function() {
    const tab = await addTab(aUrl);
    const target = await TargetFactory.forTab(tab);

    Services.prefs.setBoolPref("devtools.webaudioeditor.enabled", true);
    const toolbox = await gDevTools.showToolbox(target, "webaudioeditor");
    const panel = toolbox.getCurrentPanel();
    return { target, panel, toolbox };
  })();
}

/**
 * Close the toolbox, destroying all panels, and remove the added test tabs.
 */
function teardown(aTarget) {
  info("Destroying the web audio editor.");

  return gDevTools.closeToolbox(aTarget).then(() => {
    while (gBrowser.tabs.length > 1) {
      gBrowser.removeCurrentTab();
    }
  });
}

// Due to web audio will fire most events synchronously back-to-back,
// and we can't yield them in a chain without missing actors, this allows
// us to listen for `n` events and return a promise resolving to them.
//
// Takes a `front` object that is an event emitter, the number of
// programs that should be listened to and waited on, and an optional
// `onAdd` function that calls with the entire actors array on program link
function getN(front, eventName, count, spread) {
  const actors = [];
  info(`Waiting for ${count} ${eventName} events`);

  return new Promise((resolve) => {
    front.on(eventName, function onEvent(...args) {
      const actor = args[0];
      if (actors.length !== count) {
        actors.push(spread ? args : actor);
      }
      info(`Got ${actors.length} / ${count} ${eventName} events`);
      if (actors.length === count) {
        front.off(eventName, onEvent);
        resolve(actors);
      }
    });
  });
}

function get(front, eventName) {
  return getN(front, eventName, 1);
}
function get2(front, eventName) {
  return getN(front, eventName, 2);
}
function get3(front, eventName) {
  return getN(front, eventName, 3);
}
function getSpread(front, eventName) {
  return getN(front, eventName, 1, true);
}
function get2Spread(front, eventName) {
  return getN(front, eventName, 2, true);
}
function get3Spread(front, eventName) {
  return getN(front, eventName, 3, true);
}
function getNSpread(front, eventName, count) {
  return getN(front, eventName, count, true);
}

/**
 * Waits for the UI_GRAPH_RENDERED event to fire, but only
 * resolves when the graph was rendered with the correct count of
 * nodes and edges.
 */
function waitForGraphRendered(front, nodeCount, edgeCount, paramEdgeCount) {
  const eventName = front.EVENTS.UI_GRAPH_RENDERED;
  info(`Wait for graph rendered with ${nodeCount} nodes, ${edgeCount} edges`);

  return new Promise((resolve) => {
    front.on(eventName, function onGraphRendered(nodes, edges, pEdges) {
      const paramEdgesDone = paramEdgeCount != null ? paramEdgeCount === pEdges : true;
      info(`Got graph rendered with ${nodes} / ${nodeCount} nodes, ` +
           `${edges} / ${edgeCount} edges`);
      if (nodes === nodeCount && edges === edgeCount && paramEdgesDone) {
        front.off(eventName, onGraphRendered);
        resolve();
      }
    });
  });
}

function checkVariableView(view, index, hash, description = "") {
  info("Checking Variable View");
  const scope = view.getScopeAtIndex(index);
  const variables = Object.keys(hash);

  // If node shouldn't display any properties, ensure that the 'empty' message is
  // visible
  if (!variables.length) {
    ok(isVisible(scope.window.$("#properties-empty")),
      description + " should show the empty properties tab.");
    return;
  }

  // Otherwise, iterate over expected properties
  variables.forEach(variable => {
    const aVar = scope.get(variable);
    is(aVar.target.querySelector(".name").getAttribute("value"), variable,
      "Correct property name for " + variable);
    let value = aVar.target.querySelector(".value").getAttribute("value");

    // Cast value with JSON.parse if possible;
    // will fail when displaying Object types like "ArrayBuffer"
    // and "Float32Array", but will match the original value.
    try {
      value = JSON.parse(value);
    } catch (e) {}
    if (typeof hash[variable] === "function") {
      ok(hash[variable](value),
        "Passing property value of " + value + " for " + variable + " " + description);
    } else {
      is(value, hash[variable],
        "Correct property value of " + hash[variable] + " for " + variable + " " + description);
    }
  });
}

function modifyVariableView(win, view, index, prop, value) {
  const scope = view.getScopeAtIndex(index);
  const aVar = scope.get(prop);
  scope.expand();

  return new Promise((resolve, reject) => {
    const onParamSetSuccess = () => {
      win.off(win.EVENTS.UI_SET_PARAM_ERROR, onParamSetError);
      resolve();
    };

    const onParamSetError = () => {
      win.off(win.EVENTS.UI_SET_PARAM, onParamSetSuccess);
      reject();
    };
    win.once(win.EVENTS.UI_SET_PARAM, onParamSetSuccess);
    win.once(win.EVENTS.UI_SET_PARAM_ERROR, onParamSetError);

    // Focus and select the variable to begin editing
    win.focus();
    aVar.focus();
    EventUtils.sendKey("RETURN", win);

    // Must wait for the scope DOM to be available to receive
    // events
    executeSoon(() => {
      info("Setting " + value + " for " + prop + "....");
      for (const c of (value + "")) {
        EventUtils.synthesizeKey(c, {}, win);
      }
      EventUtils.sendKey("RETURN", win);
    });
  });
}

function findGraphEdge(win, source, target, param) {
  let selector = ".edgePaths .edgePath[data-source='" + source + "'][data-target='" + target + "']";
  if (param) {
    selector += "[data-param='" + param + "']";
  }
  return win.document.querySelector(selector);
}

function findGraphNode(win, node) {
  const selector = ".nodes > g[data-id='" + node + "']";
  return win.document.querySelector(selector);
}

function click(win, element) {
  EventUtils.sendMouseEvent({ type: "click" }, element, win);
}

function mouseOver(win, element) {
  EventUtils.sendMouseEvent({ type: "mouseover" }, element, win);
}

function command(button) {
  const ev = button.ownerDocument.createEvent("XULCommandEvent");
  ev.initCommandEvent("command", true, true, button.ownerDocument.defaultView, 0, false, false, false, false, null, 0);
  button.dispatchEvent(ev);
}

function isVisible(element) {
  return !element.getAttribute("hidden");
}

/**
 * Clicks a graph node based on actorID or passing in an element.
 * Returns a promise that resolves once UI_INSPECTOR_NODE_SET is fired and
 * the tabs have rendered, completing all RDP requests for the node.
 */
function clickGraphNode(panelWin, el, waitForToggle = false) {
  const promises = [
    once(panelWin, panelWin.EVENTS.UI_INSPECTOR_NODE_SET),
    once(panelWin, panelWin.EVENTS.UI_PROPERTIES_TAB_RENDERED),
    once(panelWin, panelWin.EVENTS.UI_AUTOMATION_TAB_RENDERED),
  ];

  if (waitForToggle) {
    promises.push(once(panelWin, panelWin.EVENTS.UI_INSPECTOR_TOGGLED));
  }

  // Use `el` as the element if it is one, otherwise
  // assume it's an ID and find the related graph node
  const element = el.tagName ? el : findGraphNode(panelWin, el);
  click(panelWin, element);

  return Promise.all(promises);
}

/**
 * Returns the primitive value of a grip's value, or the
 * original form that the string grip.type comes from.
 */
function getGripValue(value) {
  if (~["boolean", "string", "number"].indexOf(typeof value)) {
    return value;
  }

  switch (value.type) {
    case "undefined": return undefined;
    case "Infinity": return Infinity;
    case "-Infinity": return -Infinity;
    case "NaN": return NaN;
    case "-0": return -0;
    case "null": return null;
    default: return value;
  }
}

/**
 * Counts how many nodes and edges are currently in the graph.
 */
function countGraphObjects(win) {
  return {
    nodes: win.document.querySelectorAll(".nodes > .audionode").length,
    edges: win.document.querySelectorAll(".edgePaths > .edgePath").length,
  };
}

/**
* Forces cycle collection and GC, used in AudioNode destruction tests.
*/
function forceNodeCollection() {
  ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    // Kill the reference keeping stuff alive.
    content.wrappedJSObject.keepAlive = null;

    // Collect the now-deceased nodes.
    Cu.forceGC();
    Cu.forceCC();
    Cu.forceGC();
    Cu.forceCC();
  });
}

/**
 * Takes a `values` array of automation value entries,
 * looking for the value at `time` seconds, checking
 * to see if the value is close to `expected`.
 */
function checkAutomationValue(values, time, expected) {
  // Remain flexible on values as we can approximate points
  const EPSILON = 0.01;

  const value = getValueAt(values, time);
  ok(Math.abs(value - expected) < EPSILON, "Timeline value at " + time + " with value " + value + " should have value very close to " + expected);

  /**
   * Entries are ordered in `values` according to time, so if we can't find an exact point
   * on a time of interest, return the point in between the threshold. This should
   * get us a very close value.
   */
  function getValueAt(values, time) {
    for (let i = 0; i < values.length; i++) {
      if (values[i].delta === time) {
        return values[i].value;
      }
      if (values[i].delta > time) {
        return (values[i - 1].value + values[i].value) / 2;
      }
    }
    return values[values.length - 1].value;
  }
}

/**
 * Wait for all inspector tabs to complete rendering.
 */
function waitForInspectorRender(panelWin, EVENTS) {
  return Promise.all([
    once(panelWin, EVENTS.UI_PROPERTIES_TAB_RENDERED),
    once(panelWin, EVENTS.UI_AUTOMATION_TAB_RENDERED),
  ]);
}

/**
 * Takes an AudioNode type and returns it's properties (from audionode.json)
 * as keys and their default values as keys
 */
function nodeDefaultValues(nodeName) {
  const fn = NODE_CONSTRUCTORS[nodeName];

  if (typeof fn === "undefined") {
    return {};
  }

  const init = nodeName === "AudioDestinationNode" ? "destination" : `create${fn}()`;

  const definition = JSON.stringify(audioNodes[nodeName].properties);

  const evalNode = evalInDebuggee(`
    let ins = (new AudioContext()).${init};
    let props = ${definition};
    let answer = {};

    for(let k in props) {
      if (props[k].param) {
        answer[k] = ins[k].defaultValue;
      } else if (typeof ins[k] === "object" && ins[k] !== null) {
        answer[k] = ins[k].toString().slice(8, -1);
      } else {
        answer[k] = ins[k];
      }
    }
    answer;`);

  return evalNode;
}

const NODE_CONSTRUCTORS = {
  "MediaStreamAudioDestinationNode": "MediaStreamDestination",
  "AudioBufferSourceNode": "BufferSource",
  "ScriptProcessorNode": "ScriptProcessor",
  "AnalyserNode": "Analyser",
  "GainNode": "Gain",
  "DelayNode": "Delay",
  "BiquadFilterNode": "BiquadFilter",
  "WaveShaperNode": "WaveShaper",
  "PannerNode": "Panner",
  "ConvolverNode": "Convolver",
  "ChannelSplitterNode": "ChannelSplitter",
  "ChannelMergerNode": "ChannelMerger",
  "DynamicsCompressorNode": "DynamicsCompressor",
  "OscillatorNode": "Oscillator",
  "StereoPannerNode": "StereoPanner",
};
