/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");

const devtools = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
const { require } = devtools;

let { console } = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
let { EventTarget } = require("sdk/event/target");

const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
const { Class } = require("sdk/core/heritage");
const EventEmitter = require("devtools/toolkit/event-emitter");
const STRINGS_URI = "chrome://browser/locale/devtools/webaudioeditor.properties"
const L10N = new ViewHelpers.L10N(STRINGS_URI);
const Telemetry = require("devtools/shared/telemetry");
const telemetry = new Telemetry();

devtools.lazyRequireGetter(this, "LineGraphWidget",
  "devtools/shared/widgets/LineGraphWidget");

// `AUDIO_NODE_DEFINITION` defined in the controller's initialization,
// which describes all the properties of an AudioNode
let AUDIO_NODE_DEFINITION;

// Override DOM promises with Promise.jsm helpers
const { defer, all } = Cu.import("resource://gre/modules/Promise.jsm", {}).Promise;

/* Events fired on `window` to indicate state or actions*/
const EVENTS = {
  // Fired when the first AudioNode has been created, signifying
  // that the AudioContext is being used and should be tracked via the editor.
  START_CONTEXT: "WebAudioEditor:StartContext",

  // When the devtools theme changes.
  THEME_CHANGE: "WebAudioEditor:ThemeChange",

  // When the UI is reset from tab navigation.
  UI_RESET: "WebAudioEditor:UIReset",

  // When a param has been changed via the UI and successfully
  // pushed via the actor to the raw audio node.
  UI_SET_PARAM: "WebAudioEditor:UISetParam",

  // When a node is to be set in the InspectorView.
  UI_SELECT_NODE: "WebAudioEditor:UISelectNode",

  // When the inspector is finished setting a new node.
  UI_INSPECTOR_NODE_SET: "WebAudioEditor:UIInspectorNodeSet",

  // When the inspector is finished rendering in or out of view.
  UI_INSPECTOR_TOGGLED: "WebAudioEditor:UIInspectorToggled",

  // When an audio node is finished loading in the Properties tab.
  UI_PROPERTIES_TAB_RENDERED: "WebAudioEditor:UIPropertiesTabRendered",

  // When an audio node is finished loading in the Automation tab.
  UI_AUTOMATION_TAB_RENDERED: "WebAudioEditor:UIAutomationTabRendered",

  // When the Audio Context graph finishes rendering.
  // Is called with two arguments, first representing number of nodes
  // rendered, second being the number of edge connections rendering (not counting
  // param edges), followed by the count of the param edges rendered.
  UI_GRAPH_RENDERED: "WebAudioEditor:UIGraphRendered",

  // Called when the inspector splitter is moved and resized.
  UI_INSPECTOR_RESIZE: "WebAudioEditor:UIInspectorResize"
};

/**
 * The current target and the Web Audio Editor front, set by this tool's host.
 */
let gToolbox, gTarget, gFront;

/**
 * Convenient way of emitting events from the panel window.
 */
EventEmitter.decorate(this);

/**
 * DOM query helper.
 */
function $(selector, target = document) { return target.querySelector(selector); }
function $$(selector, target = document) { return target.querySelectorAll(selector); }

/**
 * Takes an iterable collection, and a hash. Return the first
 * object in the collection that matches the values in the hash.
 * From Backbone.Collection#findWhere
 * http://backbonejs.org/#Collection-findWhere
 */
function findWhere (collection, attrs) {
  let keys = Object.keys(attrs);
  for (let model of collection) {
    if (keys.every(key => model[key] === attrs[key])) {
      return model;
    }
  }
  return void 0;
}

function mixin (source, ...args) {
  args.forEach(obj => Object.keys(obj).forEach(prop => source[prop] = obj[prop]));
  return source;
}
