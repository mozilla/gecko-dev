/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { throttle } = require("resource://devtools/shared/throttle.js");

const {
  makeDebuggeeValue,
  createValueGripForTarget,
} = require("devtools/server/actors/object/utils");

const {
  TYPES,
  getResourceWatcher,
} = require("resource://devtools/server/actors/resources/index.js");
const { JSTRACER_TRACE } = TYPES;

const lazy = {};
ChromeUtils.defineESModuleGetters(
  lazy,
  {
    JSTracer: "resource://devtools/server/tracer/tracer.sys.mjs",
  },
  { global: "contextual" }
);

const {
  getActorIdForInternalSourceId,
} = require("resource://devtools/server/actors/utils/dbg-source.js");

const THROTTLING_DELAY = 250;

class ResourcesTracingListener {
  constructor({ targetActor, traceValues, traceActor }) {
    this.targetActor = targetActor;
    this.traceValues = traceValues;
    this.sourcesManager = targetActor.sourcesManager;
    this.traceActor = traceActor;

    // On workers, we don't have access to setTimeout and can't have throttling
    this.throttleEmitTraces = isWorker
      ? this.flushTraces.bind(this)
      : throttle(this.flushTraces.bind(this), THROTTLING_DELAY);
  }

  // Collect pending data to be sent to the client in various arrays,
  // each focusing on particular data type.
  // All these arrays contains arrays as elements.
  #throttledTraces = [];

  // Index of the next collected frame
  #frameIndex = 0;
  // Three level of Maps, ultimately storing frame indexes.
  // The first level of Map is keyed by source ID,
  // the second by line number,
  // the last by column number.
  // Frame objects are sent to the client and not being held in memory,
  // we only store their related indexes which are put in trace arrays.
  #frameMap = new Map();

  /**
   * Called when the tracer stops recording JS executions.
   */
  stop() {
    this.#frameIndex = 0;
    this.#frameMap.clear();
  }

  /**
   * This method is throttled and will notify all pending traces to be logged in the console
   * via the console message watcher.
   */
  flushTraces() {
    const traceWatcher = getResourceWatcher(this.targetActor, JSTRACER_TRACE);
    // Ignore the request if the frontend isn't listening to traces for that target.
    if (!traceWatcher) {
      return;
    }
    const traces = this.#throttledTraces;
    this.#throttledTraces = [];

    traceWatcher.emitTraces(traces);
  }

  /**
   * Be notified by the underlying JavaScriptTracer class
   * in case it stops by itself, instead of being stopped when the Actor's stopTracing
   * method is called by the user.
   *
   * @param {Boolean} enabled
   *        True if the tracer starts tracing, false it it stops.
   * @return {Boolean}
   *         Return true, if the JavaScriptTracer should log a message to stdout.
   */
  onTracingToggled(enabled) {
    if (!enabled) {
      this.traceActor.stopTracing();
    }
    return false;
  }

  /**
   * Called when "trace on next user interaction" is enabled, to notify the user
   * that the tracer is initialized but waiting for the user first input.
   */
  onTracingPending() {
    const consoleMessageWatcher = getResourceWatcher(
      this.targetActor,
      TYPES.CONSOLE_MESSAGE
    );
    if (consoleMessageWatcher) {
      consoleMessageWatcher.emitMessages([
        {
          arguments: [lazy.JSTracer.NEXT_INTERACTION_MESSAGE],
          styles: [],
          level: "jstracer",
          chromeContext: false,
          timeStamp: ChromeUtils.dateNow(),
        },
      ]);
    }
    return false;
  }

  /**
   * Called by JavaScriptTracer class when a new mutation happened on any DOM Element.
   *
   * @param {Object} options
   * @param {Number} options.depth
   *        Represents the depth of the frame in the call stack.
   * @param {String} options.prefix
   *        A string to be displayed as a prefix of any logged frame.
   * @param {nsIStackFrame} options.caller
   *        The JS Callsite which caused this mutation.
   * @param {String} options.type
   *        Type of DOM Mutation:
   *        - "add": Node being added,
   *        - "attributes": Node whose attributes changed,
   *        - "remove": Node being removed,
   * @param {DOMNode} options.element
   *        The DOM Node related to the current mutation.
   * @return {Boolean}
   *         Return true, if the JavaScriptTracer should log a message to stdout.
   */
  onTracingDOMMutation({ depth, prefix, type, caller, element }) {
    const dbgObj = makeDebuggeeValue(this.targetActor, element);
    const frameIndex = this.#getFrameIndex(
      null,
      null,
      caller
        ? getActorIdForInternalSourceId(this.targetActor, caller.sourceId)
        : null,
      caller?.lineNumber,
      caller?.columnNumber,
      caller?.filename
    );
    this.#throttledTraces.push([
      "dom-mutation",
      prefix,
      frameIndex,
      ChromeUtils.dateNow(),
      depth,
      type,
      createValueGripForTarget(this.targetActor, dbgObj),
    ]);
    this.throttleEmitTraces();
    return false;
  }

  /**
   * Called by JavaScriptTracer class on each step of a function call.
   *
   * @param {Object} options
   * @param {Debugger.Frame} options.frame
   *        A descriptor object for the JavaScript frame.
   * @param {Number} options.depth
   *        Represents the depth of the frame in the call stack.
   * @param {String} options.prefix
   *        A string to be displayed as a prefix of any logged frame.
   * @return {Boolean}
   *         Return true, if the JavaScriptTracer should log the step to stdout.
   */
  onTracingFrameStep({ frame, depth, prefix }) {
    const { script } = frame;
    const { lineNumber, columnNumber } = script.getOffsetMetadata(frame.offset);
    const url = script.source.url;

    // NOTE: Debugger.Script.prototype.getOffsetMetadata returns
    //       columnNumber in 1-based.
    //       Convert to 0-based, while keeping the wasm's column (1) as is.
    //       (bug 1863878)
    const columnBase = script.format === "wasm" ? 0 : 1;
    const column = columnNumber - columnBase;

    // Ignore blackboxed sources
    if (this.sourcesManager.isBlackBoxed(url, lineNumber, column)) {
      return false;
    }

    const frameIndex = this.#getFrameIndex(
      frame.implementation,
      null,
      getActorIdForInternalSourceId(this.targetActor, script.source.id),
      lineNumber,
      column,
      url
    );
    this.#throttledTraces.push([
      "step",
      prefix,
      frameIndex,
      ChromeUtils.dateNow(),
      depth,
      null,
    ]);
    this.throttleEmitTraces();

    return false;
  }

  #getFrameIndex(implementation, name, sourceId, line, column, url) {
    let perSourceMap = this.#frameMap.get(sourceId);
    if (!perSourceMap) {
      perSourceMap = new Map();
      this.#frameMap.set(sourceId, perSourceMap);
    }
    let perLineMap = perSourceMap.get(line);
    if (!perLineMap) {
      perLineMap = new Map();
      perSourceMap.set(line, perLineMap);
    }
    let frameIndex = perLineMap.get(column);

    if (frameIndex == undefined) {
      frameIndex = this.#frameIndex++;

      // Remember updating TRACER_FIELDS_INDEXES when modifying the following array:
      const frameArray = [
        "frame",
        implementation,
        name,
        sourceId,
        line,
        column,
        url,
      ];

      perLineMap.set(column, frameIndex);
      this.#throttledTraces.push(frameArray);
    }
    return frameIndex;
  }

  /**
   * Called by JavaScriptTracer class when a new JavaScript frame is executed.
   *
   * @param {Debugger.Frame} frame
   *        A descriptor object for the JavaScript frame.
   * @param {Number} depth
   *        Represents the depth of the frame in the call stack.
   * @param {String} formatedDisplayName
   *        A human readable name for the current frame.
   * @param {String} prefix
   *        A string to be displayed as a prefix of any logged frame.
   * @param {String} currentDOMEvent
   *        If this is a top level frame (depth==0), and we are currently processing
   *        a DOM Event, this will refer to the name of that DOM Event.
   *        Note that it may also refer to setTimeout and setTimeout callback calls.
   * @return {Boolean}
   *         Return true, if the JavaScriptTracer should log the frame to stdout.
   */
  onTracingFrame({
    frame,
    depth,
    formatedDisplayName,
    prefix,
    currentDOMEvent,
  }) {
    const { script } = frame;
    const { lineNumber, columnNumber } = script.getOffsetMetadata(frame.offset);
    const url = script.source.url;

    // NOTE: Debugger.Script.prototype.getOffsetMetadata returns
    //       columnNumber in 1-based.
    //       Convert to 0-based, while keeping the wasm's column (1) as is.
    //       (bug 1863878)
    const columnBase = script.format === "wasm" ? 0 : 1;
    const column = columnNumber - columnBase;

    // Ignore blackboxed sources
    if (this.sourcesManager.isBlackBoxed(url, lineNumber, column)) {
      return false;
    }

    // We may receive the currently processed DOM event (if this relates to one).
    // In this case, log a preliminary message, which looks different to highlight it.
    if (currentDOMEvent && depth == 0) {
      // Create a JSTRACER_TRACE resource with a slightly different shape
      this.#throttledTraces.push([
        "event",
        prefix,
        null,
        ChromeUtils.dateNow(),
        // Events are parent of any subsequent JS call, which has a 0 depth.
        -1,
        currentDOMEvent,
      ]);
    }

    let args = undefined,
      argNames = undefined;
    // Log arguments, but only when this feature is enabled as it introduce
    // some significant overhead in perf as well as memory as it may hold the objects in memory.
    // Also prevent trying to log function call arguments if we aren't logging a frame
    // with arguments (e.g. Debugger evaluation frames, when executing from the console)
    if (this.traceValues && frame.arguments) {
      args = [];
      for (let arg of frame.arguments) {
        // Debugger.Frame.arguments contains either a Debugger.Object or primitive object
        if (arg?.unsafeDereference) {
          arg = arg.unsafeDereference();
        }
        // Instantiate a object actor so that the tools can easily inspect these objects
        const dbgObj = makeDebuggeeValue(this.targetActor, arg);
        args.push(createValueGripForTarget(this.targetActor, dbgObj));
      }
      argNames = frame.callee.script.parameterNames;
    }

    // In order for getActorIdForInternalSourceId to work reliably,
    // we have to ensure creating a source actor for that source.
    // It happens on Google Docs that some evaled sources aren't registered?
    this.sourcesManager.getOrCreateSourceActor(script.source);

    const frameIndex = this.#getFrameIndex(
      frame.implementation,
      formatedDisplayName,
      getActorIdForInternalSourceId(this.targetActor, script.source.id),
      lineNumber,
      column,
      url
    );
    this.#throttledTraces.push([
      "enter",
      prefix,
      frameIndex,
      ChromeUtils.dateNow(),
      depth,
      args,
      argNames,
    ]);
    this.throttleEmitTraces();

    return false;
  }

  /**
   * Called by JavaScriptTracer class when a JavaScript frame exits (i.e. a function returns or throw).
   *
   * @param {Object} options
   * @param {Number} options.frameId
   *        Unique identifier for the current frame.
   *        This should match a frame notified via onTracingFrame.
   * @param {Debugger.Frame} options.frame
   *        A descriptor object for the JavaScript frame.
   * @param {Number} options.depth
   *        Represents the depth of the frame in the call stack.
   * @param {String} options.formatedDisplayName
   *        A human readable name for the current frame.
   * @param {String} options.prefix
   *        A string to be displayed as a prefix of any logged frame.
   * @param {String} options.why
   *        A string to explain why the function stopped.
   *        See tracer.sys.mjs's FRAME_EXIT_REASONS.
   * @param {Debugger.Object|primitive} options.rv
   *        The returned value. It can be the returned value, or the thrown exception.
   *        It is either a primitive object, otherwise it is a Debugger.Object for any other JS Object type.
   * @return {Boolean}
   *         Return true, if the JavaScriptTracer should log the frame to stdout.
   */
  onTracingFrameExit({
    frameId,
    frame,
    depth,
    formatedDisplayName,
    prefix,
    why,
    rv,
  }) {
    const { script } = frame;
    const { lineNumber, columnNumber } = script.getOffsetMetadata(frame.offset);
    const url = script.source.url;

    // NOTE: Debugger.Script.prototype.getOffsetMetadata returns
    //       columnNumber in 1-based.
    //       Convert to 0-based, while keeping the wasm's column (1) as is.
    //       (bug 1863878)
    const columnBase = script.format === "wasm" ? 0 : 1;
    const column = columnNumber - columnBase;

    // Ignore blackboxed sources
    if (this.sourcesManager.isBlackBoxed(url, lineNumber, column)) {
      return false;
    }

    let returnedValue = undefined;
    // Log arguments, but only when this feature is enabled as it introduce
    // some significant overhead in perf as well as memory as it may hold the objects in memory.
    if (this.traceValues) {
      // Debugger.Frame.arguments contains either a Debugger.Object or primitive object
      if (rv?.unsafeDereference) {
        rv = rv.unsafeDereference();
      }
      // Instantiate a object actor so that the tools can easily inspect these objects
      const dbgObj = makeDebuggeeValue(this.targetActor, rv);
      returnedValue = createValueGripForTarget(this.targetActor, dbgObj);
    }

    const frameIndex = this.#getFrameIndex(
      frame.implementation,
      formatedDisplayName,
      getActorIdForInternalSourceId(this.targetActor, script.source.id),
      lineNumber,
      column,
      url
    );
    this.#throttledTraces.push([
      "exit",
      prefix,
      frameIndex,
      ChromeUtils.dateNow(),
      depth,
      frameId,
      returnedValue,
      why,
    ]);
    this.throttleEmitTraces();

    return false;
  }
}

exports.ResourcesTracingListener = ResourcesTracingListener;
