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
const Services = require("Services");

///////////////////////////////////////////////////////////////////////////////
// ReplayDebugger
///////////////////////////////////////////////////////////////////////////////

// Possible preferred directions of travel.
const Direction = {
  FORWARD: "FORWARD",
  BACKWARD: "BACKWARD",
  NONE: "NONE",
};

function ReplayDebugger() {
  const existing = RecordReplayControl.registerReplayDebugger(this);
  if (existing) {
    // There is already a ReplayDebugger in existence, use that. There can only
    // be one ReplayDebugger in the process.
    return existing;
  }

  // Whether the process is currently paused.
  this._paused = false;

  // Preferred direction of travel when not explicitly resumed.
  this._direction = Direction.NONE;

  // All breakpoint positions and handlers installed by this debugger.
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

  // How many nested thread-wide paused have been entered.
  this._threadPauseCount = 0;

  // Flag set if the dispatched _performPause() call can be ignored because the
  // server entered a thread-wide pause first.
  this._cancelPerformPause = false;

  // After we are done pausing, callback describing how to resume.
  this._resumeCallback = null;

  // Handler called when hitting the beginning/end of the recording, or when
  // a time warp target has been reached.
  this.replayingOnForcedPause = null;

  // Handler called when the child pauses for any reason.
  this.replayingOnPositionChange = null;
}

// Frame index used to refer to the newest frame in the child process.
const NewestFrameIndex = -1;

ReplayDebugger.prototype = {

  /////////////////////////////////////////////////////////
  // General methods
  /////////////////////////////////////////////////////////

  replaying: true,

  canRewind: RecordReplayControl.canRewind,

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
    this._ensurePaused();
    return this._sendRequest({ type: "getContent", url });
  },

  // Send a request object to the child process, and synchronously wait for it
  // to respond.
  _sendRequest(request) {
    assert(this._paused);
    const data = RecordReplayControl.sendRequest(request);
    dumpv("SendRequest: " +
          JSON.stringify(request) + " -> " + JSON.stringify(data));
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
    assert(this._paused);
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

  /////////////////////////////////////////////////////////
  // Paused/running state
  /////////////////////////////////////////////////////////

  // Paused State Management
  //
  // The single ReplayDebugger is exclusively responsible for controlling the
  // position of the child process by keeping track of when it pauses and
  // sending it commands to resume.
  //
  // The general goal of controlling this position is to make the child process
  // execute at predictable times, similar to how it would execute if the
  // debuggee was in the same process as this one (as is the case when not
  // replaying), as described below:
  //
  // - After the child pauses, the it will only resume executing when an event
  //   loop is running that is *not* associated with the thread actor's nested
  //   pauses. As long as the thread actor has pushed a pause, the child will
  //   remain paused.
  //
  // - After the child resumes, installed breakpoint handlers will only execute
  //   when an event loop is running (which, because of the above point, cannot
  //   be associated with a thread actor's nested pause).

  replayResumeBackward() { this._resume(/* forward = */ false); },
  replayResumeForward() { this._resume(/* forward = */ true); },

  _resume(forward) {
    this._ensurePaused();
    this._setResume(() => {
      this._paused = false;
      this._direction = forward ? Direction.FORWARD : Direction.BACKWARD;
      dumpv("Resuming " + this._direction);
      RecordReplayControl.resume(forward);
      if (this._paused) {
        // If we resume and immediately pause, we are at an endpoint of the
        // recording. Force the thread to pause.
        this.replayingOnForcedPause(this.getNewestFrame());
      }
    });
  },

  replayTimeWarp(target) {
    this._ensurePaused();
    this._setResume(() => {
      this._paused = false;
      this._direction = Direction.NONE;
      dumpv("Warping " + JSON.stringify(target));
      RecordReplayControl.timeWarp(target);

      // timeWarp() doesn't return until the child has reached the target of
      // the warp, after which we force the thread to pause.
      assert(this._paused);
      this.replayingOnForcedPause(this.getNewestFrame());
    });
  },

  replayPause() {
    this._ensurePaused();

    // Cancel any pending resume.
    this._resumeCallback = null;
  },

  _ensurePaused() {
    if (!this._paused) {
      RecordReplayControl.waitUntilPaused();
      assert(this._paused);
    }
  },

  // This hook is called whenever the child has paused, which can happen
  // within a RecordReplayControl method (resume, timeWarp, waitUntilPaused) or
  // or be delivered via the event loop.
  _onPause() {
    this._paused = true;

    // The position change handler is always called on pause notifications.
    if (this.replayingOnPositionChange) {
      this.replayingOnPositionChange();
    }

    // Call _performPause() soon via the event loop to check for breakpoint
    // handlers at this point.
    this._cancelPerformPause = false;
    Services.tm.dispatchToMainThread(this._performPause.bind(this));
  },

  _performPause() {
    // The child paused at some time in the past and any breakpoint handlers
    // may still need to be called. If we've entered a thread-wide pause or
    // have already told the child to resume, don't call handlers.
    if (!this._paused || this._cancelPerformPause || this._resumeCallback) {
      return;
    }

    const point = this.replayCurrentExecutionPoint();
    dumpv("PerformPause " + JSON.stringify(point));

    if (point.position.kind == "Invalid") {
      // We paused at a checkpoint, and there are no handlers to call.
    } else {
      // Call any handlers for this point, unless one resumes execution.
      for (const { handler, position } of this._breakpoints) {
        if (RecordReplayControl.positionSubsumes(position, point.position)) {
          handler();
          assert(!this._threadPauseCount);
          if (this._resumeCallback) {
            break;
          }
        }
      }
    }

    // If no handlers entered a thread-wide pause (resetting this._direction)
    // or gave an explicit resume, continue traveling in the same direction
    // we were going when we paused.
    assert(!this._threadPauseCount);
    if (!this._resumeCallback) {
      switch (this._direction) {
      case Direction.FORWARD: this.replayResumeForward(); break;
      case Direction.BACKWARD: this.replayResumeBackward(); break;
      }
    }
  },

  // This hook is called whenever we switch between recording and replaying
  // child processes.
  _onSwitchChild() {
    // The position change handler listens to changes to the current child.
    if (this.replayingOnPositionChange) {
      // Children are paused whenever we switch between them.
      const paused = this._paused;
      this._paused = true;
      this.replayingOnPositionChange();
      this._paused = paused;
    }
  },

  replayPushThreadPause() {
    // The thread has paused so that the user can interact with it. The child
    // will stay paused until this thread-wide pause has been popped.
    assert(this._paused);
    assert(!this._resumeCallback);
    if (++this._threadPauseCount == 1) {
      // Save checkpoints near the current position in case the user rewinds.
      RecordReplayControl.markExplicitPause();

      // There is no preferred direction of travel after an explicit pause.
      this._direction = Direction.NONE;

      // Update graphics according to the current state of the child.
      this._repaint();

      // If breakpoint handlers for the pause haven't been called yet, don't
      // call them at all.
      this._cancelPerformPause = true;
    }
    const point = this.replayCurrentExecutionPoint();
    dumpv("PushPause " + JSON.stringify(point));
  },

  replayPopThreadPause() {
    dumpv("PopPause");

    // After popping the last thread-wide pause, the child can resume.
    if (--this._threadPauseCount == 0 && this._resumeCallback) {
      Services.tm.dispatchToMainThread(this._performResume.bind(this));
    }
  },

  _setResume(callback) {
    assert(this._paused);

    // Overwrite any existing resume direction.
    this._resumeCallback = callback;

    // The child can resume immediately if there is no thread-wide pause.
    if (!this._threadPauseCount) {
      Services.tm.dispatchToMainThread(this._performResume.bind(this));
    }
  },

  _performResume() {
    assert(this._paused && !this._threadPauseCount);
    if (this._resumeCallback && !this._threadPauseCount) {
      const callback = this._resumeCallback;
      this._invalidateAfterUnpause();
      this._resumeCallback = null;
      callback();
    }
  },

  // Clear out all data that becomes invalid when the child unpauses.
  _invalidateAfterUnpause() {
    this._frames.forEach(frame => frame._invalidate());
    this._frames.length = 0;

    this._objects.forEach(obj => obj._invalidate());
    this._objects.length = 0;
  },

  /////////////////////////////////////////////////////////
  // Breakpoint management
  /////////////////////////////////////////////////////////

  _setBreakpoint(handler, position, data) {
    this._ensurePaused();
    dumpv("AddBreakpoint " + JSON.stringify(position));
    RecordReplayControl.addBreakpoint(position);
    this._breakpoints.push({handler, position, data});
  },

  _clearMatchingBreakpoints(callback) {
    this._ensurePaused();
    const newBreakpoints = this._breakpoints.filter(bp => !callback(bp));
    if (newBreakpoints.length != this._breakpoints.length) {
      dumpv("ClearBreakpoints");
      RecordReplayControl.clearBreakpoints();
      for (const { position } of newBreakpoints) {
        dumpv("AddBreakpoint " + JSON.stringify(position));
        RecordReplayControl.addBreakpoint(position);
      }
    }
    this._breakpoints = newBreakpoints;
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

  // Clear OnStep and OnPop hooks for all frames.
  replayClearSteppingHooks() {
    this._clearMatchingBreakpoints(
      ({position}) => position.kind == "OnStep" || position.kind == "OnPop"
    );
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
    this._ensurePaused();
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
    this._ensurePaused();
    const data = this._sendRequest({ type: "findSources" });
    return data.map(source => this._addSource(source));
  },

  /////////////////////////////////////////////////////////
  // Object methods
  /////////////////////////////////////////////////////////

  // Objects which |forConsole| is set are objects that were logged in console
  // messages, and had their properties recorded so that they can be inspected
  // without switching to a replaying child.
  _getObject(id, forConsole) {
    if (id && !this._objects[id]) {
      const data = this._sendRequest({ type: "getObject", id });
      switch (data.kind) {
      case "Object":
        this._objects[id] = new ReplayDebuggerObject(this, data, forConsole);
        break;
      case "Environment":
        this._objects[id] = new ReplayDebuggerEnvironment(this, data);
        break;
      default:
        ThrowError("Unknown object kind");
      }
    }
    const rv = this._objects[id];
    if (forConsole) {
      rv._forConsole = true;
    }
    return rv;
  },

  _convertValue(value, forConsole) {
    if (isNonNullObject(value)) {
      if (value.object) {
        return this._getObject(value.object, forConsole);
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
        message.arguments[i] = this._convertValue(message.arguments[i],
                                                  /* forConsole = */ true);
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
                               () => { handler.call(this, this.getNewestFrame()); });
  },

  get replayingOnPopFrame() {
    return this._searchBreakpoints(({position, data}) => {
      return (position.kind == "OnPop" && !position.script) ? data : null;
    });
  },

  set replayingOnPopFrame(handler) {
    if (handler) {
      this._setBreakpoint(() => { handler.call(this, this.getNewestFrame()); },
                          { kind: "OnPop" }, handler);
    } else {
      this._clearMatchingBreakpoints(({position}) => {
        return position.kind == "OnPop" && !position.script;
      });
    }
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
    this._dbg._setBreakpoint(() => { handler.hit(this._dbg.getNewestFrame()); },
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
      this._data.arguments.map(a => this._dbg._convertValue(a));
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
    // Use setReplayingOnStep or replayClearSteppingHooks instead.
    NotAllowed();
  },

  setReplayingOnStep(handler, offsets) {
    offsets.forEach(offset => {
      this._dbg._setBreakpoint(
        () => { handler.call(this._dbg.getNewestFrame()); },
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
          const result = this._dbg._sendRequest({ type: "popFrameResult" });
          handler.call(this._dbg.getNewestFrame(),
                       this._dbg._convertCompletionValue(result));
        },
        { kind: "OnPop", script: this._data.script, frameIndex: this._data.index },
        handler);
    } else {
      // Use replayClearSteppingHooks instead.
      NotAllowed();
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

function ReplayDebuggerObject(dbg, data, forConsole) {
  this._dbg = dbg;
  this._data = data;
  this._forConsole = forConsole;
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

  get proto() {
    // Don't allow inspection of the prototypes of objects logged to the
    // console. This is a hack that prevents the object inspector from crawling
    // the object's prototype chain.
    return this._forConsole ? null : this._dbg._getObject(this._data.proto);
  },

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
      const id = this._data.id;
      const properties = this._forConsole
        ? this._dbg._sendRequest({ type: "getObjectPropertiesForConsole", id })
        : this._dbg._sendRequestAllowDiverge({ type: "getObjectProperties", id });
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

function dumpv(str) {
  //dump("[ReplayDebugger] " + str + "\n");
}

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
    ThrowError("Assertion Failed!");
  }
}

function isNonNullObject(obj) {
  return obj && (typeof obj == "object" || typeof obj == "function");
}

module.exports = ReplayDebugger;
