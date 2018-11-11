/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy */

// When recording/replaying an execution with Web Replay, Devtools server code
// runs in the middleman process instead of the recording/replaying process the
// code is interested in.
//
// This file defines replay objects analogous to those constructed by the
// C++ Debugger (Debugger, Debugger.Object, etc.), which implement similar
// methods and properties to those C++ objects. These replay objects are
// created in the middleman process, and describe things that exist in the
// recording/replaying process, inspecting them via the RecordReplayControl
// interface.

"use strict";

const RecordReplayControl = !isWorker && require("RecordReplayControl");

///////////////////////////////////////////////////////////////////////////////
// ReplayDebugger
///////////////////////////////////////////////////////////////////////////////

function ReplayDebugger() {
  RecordReplayControl.registerReplayDebugger(this);

  // All breakpoints (per BreakpointPosition) installed by this debugger.
  this._breakpoints = [];

  // All ReplayDebuggerFramees that have been created while paused at the
  // current position, indexed by their index (zero is the oldest frame, with
  // the index increasing for newer frames). These are invalidated when
  // unpausing.
  this._frames = [];

  // All ReplayDebuggerObjects and ReplayDebuggerEnvironments that have been
  // created while paused at the current position, indexed by their id. These
  // are invalidated when unpausing.
  this._objects = [];

  // All ReplayDebuggerScripts and ReplayDebuggerScriptSources that have been
  // created, indexed by their id. These stay valid even after unpausing.
  this._scripts = [];
  this._scriptSources = [];
}

// Frame index used to refer to the newest frame in the child process.
const NewestFrameIndex = -1;

ReplayDebugger.prototype = {

  /////////////////////////////////////////////////////////
  // General methods
  /////////////////////////////////////////////////////////

  replaying: true,

  canRewind: RecordReplayControl.canRewind,
  replayResumeBackward() { RecordReplayControl.resume(/* forward = */ false); },
  replayResumeForward() { RecordReplayControl.resume(/* forward = */ true); },
  replayTimeWarp: RecordReplayControl.timeWarp,

  replayPause() {
    RecordReplayControl.pause();
    this._repaint();
  },

  replayCurrentExecutionPoint() {
    return this._sendRequest({ type: "currentExecutionPoint" });
  },

  replayRecordingEndpoint() {
    return this._sendRequest({ type: "recordingEndpoint" });
  },

  replayIsRecording: RecordReplayControl.childIsRecording,

  addDebuggee() {},
  removeAllDebuggees() {},

  replayingContent(url) {
    return this._sendRequest({ type: "getContent", url });
  },

  _sendRequest(request) {
    const data = RecordReplayControl.sendRequest(request);
    //dump("SendRequest: " +
    //     JSON.stringify(request) + " -> " + JSON.stringify(data) + "\n");
    if (data.exception) {
      ThrowError(data.exception);
    }
    return data;
  },

  // Send a request that requires the child process to perform actions that
  // diverge from the recording. In such cases we want to be interacting with a
  // replaying process (if there is one), as recording child processes won't
  // provide useful responses to such requests.
  _sendRequestAllowDiverge(request) {
    RecordReplayControl.maybeSwitchToReplayingChild();
    return this._sendRequest(request);
  },

  // Update graphics according to the current state of the child process. This
  // should be done anytime we pause and allow the user to interact with the
  // debugger.
  _repaint() {
    const rv = this._sendRequestAllowDiverge({ type: "repaint" });
    if ("width" in rv && "height" in rv) {
      RecordReplayControl.hadRepaint(rv.width, rv.height);
    } else {
      RecordReplayControl.hadRepaintFailure();
    }
  },

  _setBreakpoint(handler, position, data) {
    const id = RecordReplayControl.setBreakpoint(handler, position);
    this._breakpoints.push({id, position, data});
  },

  _clearMatchingBreakpoints(callback) {
    this._breakpoints = this._breakpoints.filter(breakpoint => {
      if (callback(breakpoint)) {
        RecordReplayControl.clearBreakpoint(breakpoint.id);
        return false;
      }
      return true;
    });
  },

  _searchBreakpoints(callback) {
    for (const breakpoint of this._breakpoints) {
      const v = callback(breakpoint);
      if (v) {
        return v;
      }
    }
    return undefined;
  },

  // Getter for a breakpoint kind that has no script/offset/frameIndex.
  _breakpointKindGetter(kind) {
    return this._searchBreakpoints(({position, data}) => {
      return (position.kind == kind) ? data : null;
    });
  },

  // Setter for a breakpoint kind that has no script/offset/frameIndex.
  _breakpointKindSetter(kind, handler, callback) {
    if (handler) {
      this._setBreakpoint(callback, { kind }, handler);
    } else {
      this._clearMatchingBreakpoints(({position}) => position.kind == kind);
    }
  },

  // This is called on all ReplayDebuggers whenever the child process is about
  // to unpause. Clear out all data that is invalidated as a result.
  invalidateAfterUnpause() {
    this._frames.forEach(frame => frame._invalidate());
    this._frames.length = 0;

    this._objects.forEach(obj => obj._invalidate());
    this._objects.length = 0;
  },

  /////////////////////////////////////////////////////////
  // Script methods
  /////////////////////////////////////////////////////////

  _getScript(id) {
    if (!id) {
      return null;
    }
    const rv = this._scripts[id];
    if (rv) {
      return rv;
    }
    return this._addScript(this._sendRequest({ type: "getScript", id }));
  },

  _addScript(data) {
    if (!this._scripts[data.id]) {
      this._scripts[data.id] = new ReplayDebuggerScript(this, data);
    }
    return this._scripts[data.id];
  },

  _convertScriptQuery(query) {
    // Make a copy of the query, converting properties referring to debugger
    // things into their associated ids.
    const rv = Object.assign({}, query);
    if ("global" in query) {
      rv.global = query.global._data.id;
    }
    if ("source" in query) {
      rv.source = query.source._data.id;
    }
    return rv;
  },

  findScripts(query) {
    const data = this._sendRequest({
      type: "findScripts",
      query: this._convertScriptQuery(query),
    });
    return data.map(script => this._addScript(script));
  },

  findAllConsoleMessages() {
    const messages = this._sendRequest({ type: "findConsoleMessages" });
    return messages.map(this._convertConsoleMessage.bind(this));
  },

  /////////////////////////////////////////////////////////
  // ScriptSource methods
  /////////////////////////////////////////////////////////

  _getSource(id) {
    const source = this._scriptSources[id];
    if (source) {
      return source;
    }
    return this._addSource(this._sendRequest({ type: "getSource", id }));
  },

  _addSource(data) {
    if (!this._scriptSources[data.id]) {
      this._scriptSources[data.id] = new ReplayDebuggerScriptSource(this, data);
    }
    return this._scriptSources[data.id];
  },

  findSources() {
    const data = this._sendRequest({ type: "findSources" });
    return data.map(source => this._addSource(source));
  },

  /////////////////////////////////////////////////////////
  // Object methods
  /////////////////////////////////////////////////////////

  _getObject(id) {
    if (id && !this._objects[id]) {
      const data = this._sendRequest({ type: "getObject", id });
      switch (data.kind) {
      case "Object":
        this._objects[id] = new ReplayDebuggerObject(this, data);
        break;
      case "Environment":
        this._objects[id] = new ReplayDebuggerEnvironment(this, data);
        break;
      default:
        ThrowError("Unknown object kind");
      }
    }
    return this._objects[id];
  },

  _convertValue(value) {
    if (value && typeof value == "object") {
      if (value.object) {
        return this._getObject(value.object);
      } else if (value.special == "undefined") {
        return undefined;
      } else if (value.special == "NaN") {
        return NaN;
      } else if (value.special == "Infinity") {
        return Infinity;
      } else if (value.special == "-Infinity") {
        return -Infinity;
      }
    }
    return value;
  },

  _convertCompletionValue(value) {
    if ("return" in value) {
      return { return: this._convertValue(value.return) };
    }
    if ("throw" in value) {
      return { throw: this._convertValue(value.throw) };
    }
    ThrowError("Unexpected completion value");
    return null; // For eslint
  },

  /////////////////////////////////////////////////////////
  // Frame methods
  /////////////////////////////////////////////////////////

  _getFrame(index) {
    if (index == NewestFrameIndex) {
      if (this._frames.length) {
        return this._frames[this._frames.length - 1];
      }
    } else {
      assert(index < this._frames.length);
      if (this._frames[index]) {
        return this._frames[index];
      }
    }

    const data = this._sendRequest({ type: "getFrame", index });

    if (index == NewestFrameIndex) {
      if ("index" in data) {
        index = data.index;
      } else {
        // There are no frames on the stack.
        return null;
      }
    }

    this._frames[index] = new ReplayDebuggerFrame(this, data);
    return this._frames[index];
  },

  getNewestFrame() {
    return this._getFrame(NewestFrameIndex);
  },

  /////////////////////////////////////////////////////////
  // Console Message methods
  /////////////////////////////////////////////////////////

  _convertConsoleMessage(message) {
    // Console API message arguments need conversion to debuggee values, but
    // other contents of the message can be left alone.
    if (message.messageType == "ConsoleAPI" && message.arguments) {
      for (let i = 0; i < message.arguments.length; i++) {
        message.arguments[i] = this._convertValue(message.arguments[i]);
      }
    }
    return message;
  },

  /////////////////////////////////////////////////////////
  // Handlers
  /////////////////////////////////////////////////////////

  _getNewScript() {
    return this._addScript(this._sendRequest({ type: "getNewScript" }));
  },

  get onNewScript() { return this._breakpointKindGetter("NewScript"); },
  set onNewScript(handler) {
    this._breakpointKindSetter("NewScript", handler,
                               () => handler.call(this, this._getNewScript()));
  },

  get onEnterFrame() { return this._breakpointKindGetter("EnterFrame"); },
  set onEnterFrame(handler) {
    this._breakpointKindSetter("EnterFrame", handler,
                               () => { this._repaint();
                                       handler.call(this, this.getNewestFrame()); });
  },

  get replayingOnPopFrame() {
    return this._searchBreakpoints(({position, data}) => {
      return (position.kind == "OnPop" && !position.script) ? data : null;
    });
  },

  set replayingOnPopFrame(handler) {
    if (handler) {
      this._setBreakpoint(() => { this._repaint();
                                  handler.call(this, this.getNewestFrame()); },
                          { kind: "OnPop" }, handler);
    } else {
      this._clearMatchingBreakpoints(({position}) => {
        return position.kind == "OnPop" && !position.script;
      });
    }
  },

  get replayingOnForcedPause() {
    return this._breakpointKindGetter("ForcedPause");
  },
  set replayingOnForcedPause(handler) {
    this._breakpointKindSetter("ForcedPause", handler,
                               () => { this._repaint();
                                       handler.call(this, this.getNewestFrame()); });
  },

  get replayingOnPositionChange() {
    return this._breakpointKindGetter("PositionChange");
  },
  set replayingOnPositionChange(handler) {
    this._breakpointKindSetter("PositionChange", handler,
                               () => { handler.call(this); });
  },

  getNewConsoleMessage() {
    const message = this._sendRequest({ type: "getNewConsoleMessage" });
    return this._convertConsoleMessage(message);
  },

  get onConsoleMessage() {
    return this._breakpointKindGetter("ConsoleMessage");
  },
  set onConsoleMessage(handler) {
    this._breakpointKindSetter("ConsoleMessage", handler,
                               () => handler.call(this, this.getNewConsoleMessage()));
  },

  clearAllBreakpoints: NYI,

}; // ReplayDebugger.prototype

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerScript
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerScript(dbg, data) {
  this._dbg = dbg;
  this._data = data;
}

ReplayDebuggerScript.prototype = {
  get displayName() { return this._data.displayName; },
  get url() { return this._data.url; },
  get startLine() { return this._data.startLine; },
  get lineCount() { return this._data.lineCount; },
  get source() { return this._dbg._getSource(this._data.sourceId); },
  get sourceStart() { return this._data.sourceStart; },
  get sourceLength() { return this._data.sourceLength; },

  _forward(type, value) {
    return this._dbg._sendRequest({ type, id: this._data.id, value });
  },

  getLineOffsets(line) { return this._forward("getLineOffsets", line); },
  getOffsetLocation(pc) { return this._forward("getOffsetLocation", pc); },
  getSuccessorOffsets(pc) { return this._forward("getSuccessorOffsets", pc); },
  getPredecessorOffsets(pc) { return this._forward("getPredecessorOffsets", pc); },

  setBreakpoint(offset, handler) {
    this._dbg._setBreakpoint(() => { this._dbg._repaint();
                                     handler.hit(this._dbg.getNewestFrame()); },
                             { kind: "Break", script: this._data.id, offset },
                             handler);
  },

  clearBreakpoint(handler) {
    this._dbg._clearMatchingBreakpoints(({position, data}) => {
      return position.script == this._data.id && handler == data;
    });
  },

  get isGeneratorFunction() { NYI(); },
  get isAsyncFunction() { NYI(); },
  get format() { NYI(); },
  getChildScripts: NYI,
  getAllOffsets: NYI,
  getAllColumnOffsets: NYI,
  getBreakpoints: NYI,
  clearAllBreakpoints: NYI,
  isInCatchScope: NYI,
};

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerScriptSource
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerScriptSource(dbg, data) {
  this._dbg = dbg;
  this._data = data;
}

ReplayDebuggerScriptSource.prototype = {
  get text() { return this._data.text; },
  get url() { return this._data.url; },
  get displayURL() { return this._data.displayURL; },
  get elementAttributeName() { return this._data.elementAttributeName; },
  get introductionOffset() { return this._data.introductionOffset; },
  get introductionType() { return this._data.introductionType; },
  get sourceMapURL() { return this._data.sourceMapURL; },
  get element() { return null; },

  get introductionScript() {
    return this._dbg._getScript(this._data.introductionScript);
  },

  get binary() { NYI(); },
};

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerFrame
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerFrame(dbg, data) {
  this._dbg = dbg;
  this._data = data;
  if (this._data.arguments) {
    this._data.arguments =
      this._data.arguments.map(this._dbg._convertValue.bind(this._dbg));
  }
}

ReplayDebuggerFrame.prototype = {
  _invalidate() {
    this._data = null;
  },

  get type() { return this._data.type; },
  get callee() { return this._dbg._getObject(this._data.callee); },
  get environment() { return this._dbg._getObject(this._data.environment); },
  get generator() { return this._data.generator; },
  get constructing() { return this._data.constructing; },
  get this() { return this._dbg._convertValue(this._data.this); },
  get script() { return this._dbg._getScript(this._data.script); },
  get offset() { return this._data.offset; },
  get arguments() { return this._data.arguments; },
  get live() { return true; },

  eval(text, options) {
    const rv = this._dbg._sendRequestAllowDiverge({
      type: "frameEvaluate",
      index: this._data.index,
      text,
      options,
    });
    return this._dbg._convertCompletionValue(rv);
  },

  _positionMatches(position, kind) {
    return position.kind == kind
        && position.script == this._data.script
        && position.frameIndex == this._data.index;
  },

  get onStep() {
    return this._dbg._searchBreakpoints(({position, data}) => {
      return this._positionMatches(position, "OnStep") ? data : null;
    });
  },

  set onStep(handler) {
    if (handler) {
      // Use setReplayingOnStep instead.
      NotAllowed();
    }
    this._clearOnStepBreakpoints();
  },

  _clearOnStepBreakpoints() {
    this._dbg._clearMatchingBreakpoints(
      ({position}) => this._positionMatches(position, "OnStep")
    );
  },

  setReplayingOnStep(handler, offsets) {
    this._clearOnStepBreakpoints();
    offsets.forEach(offset => {
      this._dbg._setBreakpoint(
        () => { this._dbg._repaint();
                handler.call(this._dbg.getNewestFrame()); },
        { kind: "OnStep",
          script: this._data.script,
          offset,
          frameIndex: this._data.index },
        handler);
    });
  },

  get onPop() {
    return this._dbg._searchBreakpoints(({position, data}) => {
      return this._positionMatches(position, "OnPop") ? data : null;
    });
  },

  set onPop(handler) {
    if (handler) {
      this._dbg._setBreakpoint(() => {
          this._dbg._repaint();
          const result = this._dbg._sendRequest({ type: "popFrameResult" });
          handler.call(this._dbg.getNewestFrame(),
                       this._dbg._convertCompletionValue(result));
        },
        { kind: "OnPop", script: this._data.script, frameIndex: this._data.index },
        handler);
    } else {
      this._dbg._clearMatchingBreakpoints(
        ({position}) => this._positionMatches(position, "OnPop")
      );
    }
  },

  get older() {
    if (this._data.index == 0) {
      // This is the oldest frame.
      return null;
    }
    return this._dbg._getFrame(this._data.index - 1);
  },

  get implementation() { NYI(); },
  evalWithBindings: NYI,
};

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerObject
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerObject(dbg, data) {
  this._dbg = dbg;
  this._data = data;
  this._properties = null;
}

ReplayDebuggerObject.prototype = {
  _invalidate() {
    this._data = null;
    this._properties = null;
  },

  get callable() { return this._data.callable; },
  get isBoundFunction() { return this._data.isBoundFunction; },
  get isArrowFunction() { return this._data.isArrowFunction; },
  get isGeneratorFunction() { return this._data.isGeneratorFunction; },
  get isAsyncFunction() { return this._data.isAsyncFunction; },
  get proto() { return this._dbg._getObject(this._data.proto); },
  get class() { return this._data.class; },
  get name() { return this._data.name; },
  get displayName() { return this._data.displayName; },
  get parameterNames() { return this._data.parameterNames; },
  get script() { return this._dbg._getScript(this._data.script); },
  get environment() { return this._dbg._getObject(this._data.environment); },
  get boundTargetFunction() { return this.isBoundFunction ? NYI() : undefined; },
  get boundThis() { return this.isBoundFunction ? NYI() : undefined; },
  get boundArguments() { return this.isBoundFunction ? NYI() : undefined; },
  get global() { return this._dbg._getObject(this._data.global); },
  get isProxy() { return this._data.isProxy; },

  isExtensible() { return this._data.isExtensible; },
  isSealed() { return this._data.isSealed; },
  isFrozen() { return this._data.isFrozen; },
  unwrap() { return this.isProxy ? NYI() : this; },

  unsafeDereference() {
    // Direct access to the referent is not currently available.
    return null;
  },

  getOwnPropertyNames() {
    this._ensureProperties();
    return Object.keys(this._properties);
  },

  getOwnPropertySymbols() {
    // Symbol properties are not handled yet.
    return [];
  },

  getOwnPropertyDescriptor(name) {
    this._ensureProperties();
    const desc = this._properties[name];
    return desc ? this._convertPropertyDescriptor(desc) : null;
  },

  _ensureProperties() {
    if (!this._properties) {
      const properties = this._dbg._sendRequestAllowDiverge({
        type: "getObjectProperties",
        id: this._data.id,
      });
      this._properties = {};
      properties.forEach(({name, desc}) => { this._properties[name] = desc; });
    }
  },

  _convertPropertyDescriptor(desc) {
    const rv = Object.assign({}, desc);
    if ("value" in desc) {
      rv.value = this._dbg._convertValue(desc.value);
    }
    if ("get" in desc) {
      rv.get = this._dbg._getObject(desc.get);
    }
    if ("set" in desc) {
      rv.set = this._dbg._getObject(desc.set);
    }
    return rv;
  },

  get allocationSite() { NYI(); },
  get errorMessageName() { NYI(); },
  get errorNotes() { NYI(); },
  get errorLineNumber() { NYI(); },
  get errorColumnNumber() { NYI(); },
  get proxyTarget() { NYI(); },
  get proxyHandler() { NYI(); },
  get isPromise() { NYI(); },
  call: NYI,
  apply: NYI,
  asEnvironment: NYI,
  executeInGlobal: NYI,
  executeInGlobalWithBindings: NYI,

  makeDebuggeeValue: NotAllowed,
  preventExtensions: NotAllowed,
  seal: NotAllowed,
  freeze: NotAllowed,
  defineProperty: NotAllowed,
  defineProperties: NotAllowed,
  deleteProperty: NotAllowed,
  forceLexicalInitializationByName: NotAllowed,
};

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerEnvironment
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerEnvironment(dbg, data) {
  this._dbg = dbg;
  this._data = data;
  this._names = null;
}

ReplayDebuggerEnvironment.prototype = {
  _invalidate() {
    this._data = null;
    this._names = null;
  },

  get type() { return this._data.type; },
  get parent() { return this._dbg._getObject(this._data.parent); },
  get object() { return this._dbg._getObject(this._data.object); },
  get callee() { return this._dbg._getObject(this._data.callee); },
  get optimizedOut() { return this._data.optimizedOut; },

  _ensureNames() {
    if (!this._names) {
      const names = this._dbg._sendRequestAllowDiverge({
        type: "getEnvironmentNames",
        id: this._data.id,
      });
      this._names = {};
      names.forEach(({ name, value }) => {
        this._names[name] = this._dbg._convertValue(value);
      });
    }
  },

  names() {
    this._ensureNames();
    return Object.keys(this._names);
  },

  getVariable(name) {
    this._ensureNames();
    return this._names[name];
  },

  get inspectable() {
    // All ReplayDebugger environments are inspectable, as all compartments in
    // the replayed process are considered to be debuggees.
    return true;
  },

  find: NYI,
  setVariable: NotAllowed,
};

///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

function NYI() {
  ThrowError("Not yet implemented");
}

function NotAllowed() {
  ThrowError("Not allowed");
}

function ThrowError(msg)
{
  const error = new Error(msg);
  dump("ReplayDebugger Server Error: " + msg + " Stack: " + error.stack + "\n");
  throw error;
}

function assert(v) {
  if (!v) {
    throw new Error("Assertion Failed!");
  }
}

module.exports = ReplayDebugger;
