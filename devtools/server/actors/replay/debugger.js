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
// recording/replaying process, inspecting them via the interface provided by
// control.js.

"use strict";

const RecordReplayControl = !isWorker && require("RecordReplayControl");
const Services = require("Services");
const ChromeUtils = require("ChromeUtils");

ChromeUtils.defineModuleGetter(
  this,
  "positionSubsumes",
  "resource://devtools/shared/execution-point-utils.js"
);

loader.lazyRequireGetter(
  this,
  "ReplayInspector",
  "devtools/server/actors/replay/inspector"
);

///////////////////////////////////////////////////////////////////////////////
// ReplayDebugger
///////////////////////////////////////////////////////////////////////////////

// Possible preferred directions of travel.
const Direction = {
  FORWARD: "FORWARD",
  BACKWARD: "BACKWARD",
  NONE: "NONE",
};

// Pool of ReplayDebugger things that are grouped together and can refer to each
// other. Many things --- frames, objects, environments --- are specific to
// a pool and cannot be used in any other context. Normally a pool is associated
// with some point at which the debugger paused, but they may also be associated
// with the values in a console or logpoint message.
function ReplayPool(dbg, pauseData) {
  this.dbg = dbg;

  // All ReplayDebuggerFramees that have been created for this pool, indexed by
  // their index (zero is the oldest frame, with the index increasing for newer
  // frames).
  this.frames = [];

  // All ReplayDebuggerObjects and ReplayDebuggerEnvironments that are
  // associated with this pool, indexed by their id.
  this.objects = [];

  if (pauseData) {
    this.addPauseData(pauseData);
  }
}

ReplayPool.prototype = {
  getObject(id) {
    if (id && !this.objects[id]) {
      if (this != this.dbg._pool) {
        return null;
      }
      const data = this.dbg._sendRequest({ type: "getObject", id });
      this.addObject(data);
    }
    return this.objects[id];
  },

  addObject(data) {
    switch (data.kind) {
      case "Object":
        this.objects[data.id] = new ReplayDebuggerObject(this, data);
        break;
      case "Environment":
        this.objects[data.id] = new ReplayDebuggerEnvironment(this, data);
        break;
      default:
        ThrowError("Unknown object kind");
    }
  },

  getFrame(index) {
    if (index == NewestFrameIndex) {
      if (this.frames.length) {
        return this.frames[this.frames.length - 1];
      }
    } else {
      assert(index < this.frames.length);
      if (this.frames[index]) {
        return this.frames[index];
      }
    }

    assert(this == this.dbg._pool);
    const data = this.dbg._sendRequest({ type: "getFrame", index });

    if (index == NewestFrameIndex) {
      if ("index" in data) {
        index = data.index;
      } else {
        // There are no frames on the stack.
        return null;
      }
    }

    this.frames[index] = new ReplayDebuggerFrame(this, data);
    return this.frames[index];
  },

  addPauseData(pauseData) {
    for (const { data, preview } of Object.values(pauseData.objects)) {
      if (!this.objects[data.id]) {
        this.addObject(data);
      }
      this.getObject(data.id)._preview = {
        ...preview,
        properties: mapify(preview.properties),
        callResults: mapify(preview.callResults),
      };
    }

    for (const { data, names } of Object.values(pauseData.environments)) {
      if (!this.objects[data.id]) {
        this.addObject(data);
      }
      this.getObject(data.id)._setNames(names);
    }

    if (pauseData.frames) {
      for (const frame of pauseData.frames) {
        this.frames[frame.index] = new ReplayDebuggerFrame(this, frame);
      }
    }

    if (pauseData.popFrameResult) {
      this.popFrameResult = this.convertCompletionValue(
        pauseData.popFrameResult
      );
    }
  },

  convertValue(value) {
    if (isNonNullObject(value)) {
      if (value.object) {
        return this.getObject(value.object);
      }
      switch (value.special) {
        case "undefined":
          return undefined;
        case "Infinity":
          return Infinity;
        case "-Infinity":
          return -Infinity;
        case "NaN":
          return NaN;
        case "0":
          return -0;
      }
    }
    return value;
  },

  convertCompletionValue(value) {
    if ("return" in value) {
      return { return: this.convertValue(value.return) };
    }
    if ("throw" in value) {
      return {
        throw: this.convertValue(value.throw),
        stack: value.stack,
      };
    }
    ThrowError("Unexpected completion value");
    return null; // For eslint
  },
};

function ReplayDebugger() {
  const existing = RecordReplayControl.registerReplayDebugger(this);
  if (existing) {
    // There is already a ReplayDebugger in existence, use that. There can only
    // be one ReplayDebugger in the process.
    return existing;
  }

  // We should have been connected to control.js by the call above.
  assert(this._control);

  // Preferred direction of travel when not explicitly resumed.
  this._direction = Direction.NONE;

  // All breakpoint positions and handlers installed by this debugger.
  this._breakpoints = [];

  // The current pool of pause-local state.
  this._pool = new ReplayPool(this);

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
    return this._control.lastPausePoint();
  },

  replayFramePositions(point) {
    return this._control.findFrameSteps(point);
  },

  replayRecordingEndpoint() {
    return this._control.recordingEndpoint();
  },

  replayIsRecording() {
    return this._control.childIsRecording();
  },

  replayUnscannedRegions() {
    return this._control.unscannedRegions();
  },

  replayCachedPoints() {
    return this._control.cachedPoints();
  },

  replayDebuggerRequests() {
    return this._control.debuggerRequests();
  },

  addDebuggee() {},
  removeAllDebuggees() {},

  replayingContent(url) {
    return this._sendRequestMainChild({ type: "getContent", url });
  },

  _processResponse(request, response, divergeResponse) {
    dumpv(`SendRequest: ${stringify(request)} -> ${stringify(response)}`);
    if (response.unhandledDivergence) {
      if (divergeResponse) {
        return divergeResponse;
      }
      ThrowError(`Unhandled recording divergence in ${request.type}`);
    }
    return response;
  },

  // Send a request object to the child process, and synchronously wait for it
  // to respond. divergeResponse must be specified for requests that can diverge
  // from the recording and which we want to recover gracefully.
  _sendRequest(request, divergeResponse) {
    const response = this._control.sendRequest(request);
    return this._processResponse(request, response, divergeResponse);
  },

  // Send a request that requires the child process to perform actions that
  // diverge from the recording. In such cases we want to be interacting with a
  // replaying process (if there is one), as recording child processes won't
  // provide useful responses to such requests.
  _sendRequestAllowDiverge(request, divergeResponse) {
    this._control.maybeSwitchToReplayingChild();
    return this._sendRequest(request, divergeResponse);
  },

  _sendRequestMainChild(request) {
    const response = this._control.sendRequestMainChild(request);
    return this._processResponse(request, response);
  },

  getDebuggees() {
    return [];
  },

  replayGetExecutionPointPosition({ position }) {
    const script = this._getScript(position.script);
    if (position.kind == "EnterFrame") {
      return { script, offset: script.mainOffset };
    }
    return { script, offset: position.offset };
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

  get _paused() {
    return !!this._control.pausePoint();
  },

  replayResumeBackward() {
    this._resume(/* forward = */ false);
  },
  replayResumeForward() {
    this._resume(/* forward = */ true);
  },

  _resume(forward) {
    this._ensurePaused();
    this._setResume(() => {
      this._direction = forward ? Direction.FORWARD : Direction.BACKWARD;
      dumpv("Resuming " + this._direction);
      this._control.resume(forward);
    });
  },

  // Called when replaying and hitting the beginning or end of recording.
  _hitRecordingBoundary() {
    this._capturePauseData();
    this.replayingOnForcedPause(this.getNewestFrame());
  },

  replayTimeWarp(target) {
    this._ensurePaused();
    this._setResume(() => {
      this._direction = Direction.NONE;
      dumpv("Warping " + JSON.stringify(target));
      this._control.timeWarp(target);

      // timeWarp() doesn't return until the child has reached the target of
      // the warp, after which we force the thread to pause.
      assert(this._paused);
      this._capturePauseData();
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
      this._control.waitUntilPaused();
      assert(this._paused);
    }
  },

  // This hook is called whenever the child has paused, which can happen
  // within a control method (resume, timeWarp, waitUntilPaused) or be
  // delivered via the event loop.
  _onPause() {
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

    if (!point.position) {
      // We paused at a checkpoint, and there are no handlers to call.
    } else {
      // Call any handlers for this point, unless one resumes execution.
      for (const { handler, position } of this._breakpoints) {
        if (positionSubsumes(position, point.position)) {
          handler();
          assert(!this._threadPauseCount);
          if (this._resumeCallback) {
            break;
          }
        }
      }

      if (
        this._control.isPausedAtDebuggerStatement() &&
        this.onDebuggerStatement
      ) {
        this._capturePauseData();
        this.onDebuggerStatement(this.getNewestFrame());
      }
    }

    // If no handlers entered a thread-wide pause (resetting this._direction)
    // or gave an explicit resume, continue traveling in the same direction
    // we were going when we paused.
    assert(!this._threadPauseCount);
    if (!this._resumeCallback) {
      switch (this._direction) {
        case Direction.FORWARD:
          this.replayResumeForward();
          break;
        case Direction.BACKWARD:
          this.replayResumeBackward();
          break;
      }
    }
  },

  // This hook is called whenever control state changes which affects something
  // the position change handler listens to (more than just position changes,
  // alas).
  _callOnPositionChange() {
    if (this.replayingOnPositionChange) {
      this.replayingOnPositionChange();
    }
  },

  replayPushThreadPause() {
    // The thread has paused so that the user can interact with it. The child
    // will stay paused until this thread-wide pause has been popped.
    this._ensurePaused();
    assert(!this._resumeCallback);
    if (++this._threadPauseCount == 1) {
      // There is no preferred direction of travel after an explicit pause.
      this._direction = Direction.NONE;

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
    this._ensurePaused();
    if (this._resumeCallback && !this._threadPauseCount) {
      const callback = this._resumeCallback;
      this._invalidateAfterUnpause();
      this._resumeCallback = null;
      callback();
    }
  },

  replayPaint(data) {
    this._control.paint(data);
  },

  replayPaintCurrentPoint() {
    if (this.replayIsRecording()) {
      return RecordReplayControl.restoreMainGraphics();
    }

    const point = this._control.lastPausePoint();
    return this._control.paint(point);
  },

  // Clear out all data that becomes invalid when the child unpauses.
  _invalidateAfterUnpause() {
    this._pool = new ReplayPool(this);
  },

  // Fill in the debugger with (hopefully) all data the client/server need to
  // pause at the current location. This also updates graphics to match the
  // current location.
  _capturePauseData() {
    if (this._pool.frames.length) {
      return;
    }

    const pauseData = this._control.getPauseDataAndRepaint();
    if (!pauseData.frames) {
      return;
    }

    for (const data of Object.values(pauseData.scripts)) {
      this._addScript(data);
    }

    for (const { scriptId, offset, metadata } of pauseData.offsetMetadata) {
      if (this._scripts[scriptId]) {
        const script = this._getScript(scriptId);
        script._addOffsetMetadata(offset, metadata);
      }
    }

    this._pool.addPauseData(pauseData);
  },

  _virtualConsoleLog(position, text, condition, callback) {
    dumpv(`AddLogpoint ${JSON.stringify(position)} ${text} ${condition}`);
    this._control.addLogpoint({ position, text, condition, callback });
  },

  /////////////////////////////////////////////////////////
  // Breakpoint management
  /////////////////////////////////////////////////////////

  _setBreakpoint(handler, position, data) {
    dumpv(`AddBreakpoint ${JSON.stringify(position)}`);
    this._control.addBreakpoint(position);
    this._breakpoints.push({ handler, position, data });
  },

  _clearMatchingBreakpoints(callback) {
    const newBreakpoints = this._breakpoints.filter(bp => !callback(bp));
    if (newBreakpoints.length != this._breakpoints.length) {
      dumpv("ClearBreakpoints");
      this._control.clearBreakpoints();
      for (const { position } of newBreakpoints) {
        dumpv("AddBreakpoint " + JSON.stringify(position));
        this._control.addBreakpoint(position);
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
    return this._searchBreakpoints(({ position, data }) => {
      return position.kind == kind ? data : null;
    });
  },

  // Setter for a breakpoint kind that has no script/offset/frameIndex.
  _breakpointKindSetter(kind, handler, callback) {
    if (handler) {
      this._setBreakpoint(callback, { kind }, handler);
    } else {
      this._clearMatchingBreakpoints(({ position }) => position.kind == kind);
    }
  },

  // Clear OnStep and OnPop hooks for all frames.
  replayClearSteppingHooks() {
    this._clearMatchingBreakpoints(
      ({ position }) => position.kind == "OnStep" || position.kind == "OnPop"
    );
  },

  /////////////////////////////////////////////////////////
  // Script methods
  /////////////////////////////////////////////////////////

  _getScript(id) {
    if (!id) {
      return undefined;
    }
    const rv = this._scripts[id];
    if (rv) {
      return rv;
    }
    return this._addScript(
      this._sendRequestMainChild({ type: "getScript", id })
    );
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
      // Script queries might be sent to a different process from the one which
      // is paused at the current point and which we are interacting with.
      NYI();
    }
    if ("source" in query) {
      rv.source = query.source._data.id;
    }
    return rv;
  },

  findScripts(query) {
    const data = this._sendRequestMainChild({
      type: "findScripts",
      query: this._convertScriptQuery(query),
    });
    return data.map(script => this._addScript(script));
  },

  _onNewScript(data) {
    if (this.onNewScript) {
      const script = this._addScript(data);
      this.onNewScript(script);
    }
  },

  /////////////////////////////////////////////////////////
  // ScriptSource methods
  /////////////////////////////////////////////////////////

  _getSource(id) {
    const source = this._scriptSources[id];
    if (source) {
      return source;
    }
    return this._addSource(
      this._sendRequestMainChild({ type: "getSource", id })
    );
  },

  _addSource(data) {
    if (!this._scriptSources[data.id]) {
      this._scriptSources[data.id] = new ReplayDebuggerScriptSource(this, data);
    }
    return this._scriptSources[data.id];
  },

  findSources() {
    const data = this._sendRequestMainChild({ type: "findSources" });
    return data.map(source => this._addSource(source));
  },

  findSourceURLs() {
    return this.findSources().map(source => source.url);
  },

  adoptSource(source) {
    assert(source._dbg == this);
    return source;
  },

  /////////////////////////////////////////////////////////
  // Object methods
  /////////////////////////////////////////////////////////

  // Convert a value for sending to the child.
  _convertValueForChild(value) {
    if (isNonNullObject(value)) {
      assert(value instanceof ReplayDebuggerObject);
      assert(value._pool == this._pool);
      return { object: value._data.id };
    } else if (
      value === undefined ||
      value == Infinity ||
      value == -Infinity ||
      Object.is(value, NaN) ||
      Object.is(value, -0)
    ) {
      return { special: "" + value };
    }
    return value;
  },

  /////////////////////////////////////////////////////////
  // Frame methods
  /////////////////////////////////////////////////////////

  getNewestFrame() {
    return this._pool.getFrame(NewestFrameIndex);
  },

  /////////////////////////////////////////////////////////
  // Console Message methods
  /////////////////////////////////////////////////////////

  _convertConsoleMessage(message) {
    // Console API message arguments need conversion to debuggee values, but
    // other contents of the message can be left alone.
    if (message.messageType == "ConsoleAPI" && message.arguments) {
      // Each console message has its own pool of referenced objects.
      const pool = new ReplayPool(this, message.argumentsData);
      for (let i = 0; i < message.arguments.length; i++) {
        message.arguments[i] = pool.convertValue(message.arguments[i]);
      }
    }

    return message;
  },

  _newConsoleMessage(message) {
    if (this.onConsoleMessage) {
      this.onConsoleMessage(this._convertConsoleMessage(message));
    }
  },

  findAllConsoleMessages() {
    const messages = this._sendRequestMainChild({
      type: "findConsoleMessages",
    });
    return messages.map(this._convertConsoleMessage.bind(this));
  },

  /////////////////////////////////////////////////////////
  // Event Breakpoint methods
  /////////////////////////////////////////////////////////

  replaySetActiveEventBreakpoints(events, callback) {
    this._control.setActiveEventBreakpoints(
      events,
      (point, result, resultData) => {
        const pool = new ReplayPool(this, resultData);
        const converted = result.map(v => pool.convertValue(v));
        callback(point, converted);
      }
    );
  },

  /////////////////////////////////////////////////////////
  // Handlers
  /////////////////////////////////////////////////////////

  get onEnterFrame() {
    return this._breakpointKindGetter("EnterFrame");
  },
  set onEnterFrame(handler) {
    this._breakpointKindSetter("EnterFrame", handler, () => {
      this._capturePauseData();
      handler.call(this, this.getNewestFrame());
    });
  },

  clearAllBreakpoints: NYI,
}; // ReplayDebugger.prototype

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerScript
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerScript(dbg, data) {
  this._dbg = dbg;
  this._data = data;
  this._offsetMetadata = [];
}

ReplayDebuggerScript.prototype = {
  get displayName() {
    return this._data.displayName;
  },
  get url() {
    return this._data.url;
  },
  get startLine() {
    return this._data.startLine;
  },
  get lineCount() {
    return this._data.lineCount;
  },
  get source() {
    return this._dbg._getSource(this._data.sourceId);
  },
  get sourceStart() {
    return this._data.sourceStart;
  },
  get sourceLength() {
    return this._data.sourceLength;
  },
  get format() {
    return this._data.format;
  },
  get mainOffset() {
    return this._data.mainOffset;
  },

  _forward(type, value) {
    return this._dbg._sendRequestMainChild({ type, id: this._data.id, value });
  },

  getLineOffsets(line) {
    return this._forward("getLineOffsets", line);
  },
  getOffsetLocation(pc) {
    assert(pc !== undefined);
    return this._forward("getOffsetLocation", pc);
  },
  getSuccessorOffsets(pc) {
    return this._forward("getSuccessorOffsets", pc);
  },
  getPredecessorOffsets(pc) {
    return this._forward("getPredecessorOffsets", pc);
  },
  getAllColumnOffsets() {
    return this._forward("getAllColumnOffsets");
  },
  getPossibleBreakpoints(query) {
    return this._forward("getPossibleBreakpoints", query);
  },
  getPossibleBreakpointOffsets(query) {
    return this._forward("getPossibleBreakpointOffsets", query);
  },

  getOffsetMetadata(pc) {
    if (!this._offsetMetadata[pc]) {
      this._addOffsetMetadata(pc, this._forward("getOffsetMetadata", pc));
    }
    return this._offsetMetadata[pc];
  },

  _addOffsetMetadata(pc, metadata) {
    this._offsetMetadata[pc] = metadata;
  },

  setBreakpoint(offset, handler) {
    this._dbg._setBreakpoint(
      () => {
        this._dbg._capturePauseData();
        handler.hit(this._dbg.getNewestFrame());
      },
      { kind: "Break", script: this._data.id, offset },
      handler
    );
  },

  clearBreakpoint(handler) {
    this._dbg._clearMatchingBreakpoints(({ position, data }) => {
      return position.script == this._data.id && handler == data;
    });
  },

  replayVirtualConsoleLog(offset, text, condition, callback) {
    this._dbg._virtualConsoleLog(
      { kind: "Break", script: this._data.id, offset },
      text,
      condition,
      (point, result, resultData) => {
        const pool = new ReplayPool(this._dbg, resultData);
        const converted = result.map(v => pool.convertValue(v));
        callback(point, converted);
      }
    );
  },

  get isGeneratorFunction() {
    NYI();
  },
  get isAsyncFunction() {
    NYI();
  },
  getChildScripts: NYI,
  getAllOffsets: NYI,
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
  get text() {
    return this._data.text;
  },
  get url() {
    return this._data.url;
  },
  get displayURL() {
    return this._data.displayURL;
  },
  get elementAttributeName() {
    return this._data.elementAttributeName;
  },
  get introductionOffset() {
    return this._data.introductionOffset;
  },
  get introductionType() {
    return this._data.introductionType;
  },
  get sourceMapURL() {
    return this._data.sourceMapURL;
  },
  get element() {
    return null;
  },

  get introductionScript() {
    return this._dbg._getScript(this._data.introductionScript);
  },

  get binary() {
    NYI();
  },
};

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerFrame
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerFrame(pool, data) {
  this._dbg = pool.dbg;
  this._pool = pool;
  this._data = data;
  if (this._data.arguments) {
    this._arguments = this._data.arguments.map(a => this._pool.convertValue(a));
  }
}

ReplayDebuggerFrame.prototype = {
  get type() {
    return this._data.type;
  },
  get callee() {
    return this._pool.getObject(this._data.callee);
  },
  get environment() {
    return this._pool.getObject(this._data.environment);
  },
  get generator() {
    return this._data.generator;
  },
  get constructing() {
    return this._data.constructing;
  },
  get this() {
    return this._pool.convertValue(this._data.this);
  },
  get script() {
    return this._dbg._getScript(this._data.script);
  },
  get offset() {
    return this._data.offset;
  },
  get arguments() {
    assert(this._data);
    return this._arguments;
  },
  get live() {
    return true;
  },

  eval(text, options) {
    assert(this._pool == this._dbg._pool);
    const rv = this._dbg._sendRequestAllowDiverge(
      {
        type: "frameEvaluate",
        index: this._data.index,
        text,
        options,
      },
      { throw: "Recording divergence in frameEvaluate" }
    );
    return this._pool.convertCompletionValue(rv);
  },

  _positionMatches(position, kind) {
    return (
      position.kind == kind &&
      position.script == this._data.script &&
      position.frameIndex == this._data.index
    );
  },

  get onStep() {
    return this._dbg._searchBreakpoints(({ position, data }) => {
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
        () => {
          this._dbg._capturePauseData();
          handler.call(this._dbg.getNewestFrame());
        },
        {
          kind: "OnStep",
          script: this._data.script,
          offset,
          frameIndex: this._data.index,
        },
        handler
      );
    });
  },

  get onPop() {
    return this._dbg._searchBreakpoints(({ position, data }) => {
      return this._positionMatches(position, "OnPop") ? data : null;
    });
  },

  set onPop(handler) {
    if (handler) {
      this._dbg._setBreakpoint(
        () => {
          this._dbg._capturePauseData();
          handler.call(
            this._dbg.getNewestFrame(),
            this._dbg._pool.popFrameResult
          );
        },
        {
          kind: "OnPop",
          script: this._data.script,
          frameIndex: this._data.index,
        },
        handler
      );
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
    return this._pool.getFrame(this._data.index - 1);
  },

  get implementation() {
    NYI();
  },
  evalWithBindings: NYI,
};

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerObject
///////////////////////////////////////////////////////////////////////////////

// See replay.js
const PropertyLevels = {
  BASIC: 1,
  FULL: 2,
};

function ReplayDebuggerObject(pool, data) {
  this._dbg = pool.dbg;
  this._pool = pool;
  this._data = data;
  this._preview = null;
  this._properties = null;
  this._containerContents = null;
}

ReplayDebuggerObject.prototype = {
  toString() {
    const id = this._data ? this._data.id : "INVALID";
    return `ReplayDebugger.Object #${id}`;
  },

  get callable() {
    return this._data.callable;
  },
  get isBoundFunction() {
    return this._data.isBoundFunction;
  },
  get isArrowFunction() {
    return this._data.isArrowFunction;
  },
  get isGeneratorFunction() {
    return this._data.isGeneratorFunction;
  },
  get isAsyncFunction() {
    return this._data.isAsyncFunction;
  },
  get class() {
    return this._data.class;
  },
  get name() {
    return this._data.name;
  },
  get displayName() {
    return this._data.displayName;
  },
  get parameterNames() {
    return this._data.parameterNames;
  },
  get script() {
    return this._dbg._getScript(this._data.script);
  },
  get environment() {
    return this._pool.getObject(this._data.environment);
  },
  get isProxy() {
    return this._data.isProxy;
  },
  get proto() {
    return this._pool.getObject(this._data.proto);
  },

  isExtensible() {
    return this._data.isExtensible;
  },
  isSealed() {
    return this._data.isSealed;
  },
  isFrozen() {
    return this._data.isFrozen;
  },

  unsafeDereference() {
    if (this.class == "Array") {
      // ReplayInspector converts arrays to objects in this process, which we
      // don't want to happen.
      return null;
    }

    return ReplayInspector.wrapObject(this);
  },

  getOwnPropertyNames() {
    if (this._preview && this._preview.level >= PropertyLevels.FULL) {
      // The preview will include all properties of the object.
      return this.getEnumerableOwnPropertyNamesForPreview();
    }
    this._ensureProperties();
    return [...this._properties.keys()];
  },

  getEnumerableOwnPropertyNamesForPreview() {
    if (this._preview && this._preview.level >= PropertyLevels.BASIC) {
      if (!this._preview.properties) {
        return [];
      }
      return [...this._preview.properties.keys()];
    }
    return this.getOwnPropertyNames();
  },

  getOwnPropertyNamesCount() {
    if (this._preview) {
      return this._preview.ownPropertyNamesCount;
    }
    return this.getOwnPropertyNames().length;
  },

  getOwnPropertySymbols() {
    // Symbol properties are not handled yet.
    return [];
  },

  getOwnPropertyDescriptor(name) {
    name = name.toString();
    if (this._preview && this._preview.properties) {
      const desc = this._preview.properties.get(name);
      if (desc || this._preview.level == PropertyLevels.FULL) {
        return this._convertPropertyDescriptor(desc);
      }
    }
    this._ensureProperties();
    return this._convertPropertyDescriptor(this._properties.get(name));
  },

  _ensureProperties() {
    if (!this._properties) {
      if (this._pool != this._dbg._pool) {
        this._properties = mapify([]);
        return;
      }
      const id = this._data.id;
      const { properties } = this._dbg._sendRequestAllowDiverge(
        { type: "getObjectProperties", id },
        []
      );
      this._properties = mapify(properties);
    }
  },

  _convertPropertyDescriptor(desc) {
    if (!desc) {
      return undefined;
    }
    const rv = Object.assign({}, desc);
    if ("value" in desc) {
      rv.value = this._pool.convertValue(desc.value);
    }
    if ("get" in desc) {
      rv.get = this._pool.getObject(desc.get);
    }
    if ("set" in desc) {
      rv.set = this._pool.getObject(desc.set);
    }
    return rv;
  },

  containerContents(forPreview = false) {
    let contents;
    if (forPreview && this._preview && this._preview.containerContents) {
      contents = this._preview.containerContents;
    } else {
      if (!this._containerContents) {
        assert(this._pool == this._dbg._pool);
        const id = this._data.id;
        this._containerContents = this._dbg._sendRequestAllowDiverge(
          { type: "getObjectContainerContents", id },
          []
        );
      }
      contents = this._containerContents;
    }
    return contents.map(value => {
      // Watch for [key, value] pairs in maps.
      if (value.length == 2) {
        return value.map(v => this._pool.convertValue(v));
      }
      return this._pool.convertValue(value);
    });
  },

  replayHasCallResult(name) {
    return (
      this._preview &&
      this._preview.callResults &&
      this._preview.callResults.has(name)
    );
  },

  replayCallResult(name) {
    const value = this._preview.callResults.get(name);
    return this._pool.convertValue(value);
  },

  unwrap() {
    if (!this.isProxy) {
      return this;
    }
    return this._pool.convertValue(this._data.proxyUnwrapped);
  },

  get proxyTarget() {
    return this._pool.convertValue(this._data.proxyTarget);
  },

  get proxyHandler() {
    return this._pool.convertValue(this._data.proxyHandler);
  },

  get boundTargetFunction() {
    if (this.isBoundFunction) {
      return this._pool.getObject(this._data.boundTargetFunction);
    }
    return undefined;
  },

  get boundThis() {
    if (this.isBoundFunction) {
      return this._pool.convertValue(this._data.boundThis);
    }
    return undefined;
  },

  get boundArguments() {
    if (this.isBoundFunction) {
      return this._pool.getObject(this._data.boundArguments);
    }
    return undefined;
  },

  call(thisv, ...args) {
    return this.apply(thisv, args);
  },

  apply(thisv, args) {
    if (this._pool != this._dbg._pool) {
      return undefined;
    }

    thisv = this._dbg._convertValueForChild(thisv);
    args = (args || []).map(v => this._dbg._convertValueForChild(v));

    const rv = this._dbg._sendRequestAllowDiverge(
      {
        type: "objectApply",
        id: this._data.id,
        thisv,
        args,
      },
      { throw: "Recording divergence in objectApply" }
    );
    return this._pool.convertCompletionValue(rv);
  },

  get allocationSite() {
    NYI();
  },
  get errorMessageName() {
    return this._data.errorMessageName;
  },
  get errorNotes() {
    return this._data.errorNotes;
  },
  get errorLineNumber() {
    return this._data.errorLineNumber;
  },
  get errorColumnNumber() {
    return this._data.errorColumnNumber;
  },
  get isPromise() {
    NYI();
  },
  asEnvironment: NYI,
  executeInGlobal: NYI,
  executeInGlobalWithBindings: NYI,

  getTypedArrayLength() {
    return this._data.typedArrayLength;
  },

  makeDebuggeeValue(obj) {
    if (obj instanceof ReplayDebuggerObject) {
      return obj;
    }
    const rv = ReplayInspector.unwrapObject(obj);
    if (rv) {
      return rv;
    }
    ThrowError("Can't make debuggee value");
    return null; // For eslint
  },

  replayIsInstance(name) {
    return this._data.isInstance == name;
  },

  preventExtensions: NotAllowed,
  seal: NotAllowed,
  freeze: NotAllowed,
  defineProperty: NotAllowed,
  defineProperties: NotAllowed,
  deleteProperty: NotAllowed,
  forceLexicalInitializationByName: NotAllowed,
};

ReplayDebugger.Object = ReplayDebuggerObject;

///////////////////////////////////////////////////////////////////////////////
// ReplayDebuggerEnvironment
///////////////////////////////////////////////////////////////////////////////

function ReplayDebuggerEnvironment(pool, data) {
  this._dbg = pool.dbg;
  this._pool = pool;
  this._data = data;
  this._names = null;
}

ReplayDebuggerEnvironment.prototype = {
  get type() {
    return this._data.type;
  },
  get parent() {
    return this._pool.getObject(this._data.parent);
  },
  get object() {
    return this._pool.getObject(this._data.object);
  },
  get callee() {
    return this._pool.getObject(this._data.callee);
  },
  get optimizedOut() {
    return this._data.optimizedOut;
  },

  _setNames(names) {
    this._names = {};
    names.forEach(({ name, value }) => {
      this._names[name] = this._pool.convertValue(value);
    });
  },

  _ensureNames() {
    if (!this._names) {
      assert(this._pool == this._dbg._pool);
      const names = this._dbg._sendRequestAllowDiverge(
        {
          type: "getEnvironmentNames",
          id: this._data.id,
        },
        []
      );
      this._setNames(names);
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

function ThrowError(msg) {
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

function stringify(object) {
  const str = JSON.stringify(object);
  if (str.length >= 4096) {
    return `${str.substr(0, 4096)} TRIMMED ${str.length}`;
  }
  return str;
}

function mapify(object) {
  if (!object) {
    return undefined;
  }
  const map = new Map();
  for (const key of Object.keys(object)) {
    map.set(key, object[key]);
  }
  return map;
}

module.exports = ReplayDebugger;
