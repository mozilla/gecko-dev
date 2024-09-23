/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// The functions in the class use standard functions called from tracer.js but we want to keep the
// arguments intact.
/* eslint "no-unused-vars": ["error", {args: "none"} ]*/

class StdoutTracingListener {
  constructor({ targetActor, traceValues, traceActor }) {
    this.targetActor = targetActor;
    this.traceValues = traceValues;
    this.sourcesManager = targetActor.sourcesManager;
    this.traceActor = traceActor;
  }

  /**
   * Called when the tracer stops recording JS executions.
   */
  stop() {
    // The stdout output is simple enough to not need to do any cleanup.
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
    // Delegate to JavaScriptTracer to log to stdout
    return true;
  }

  /**
   * Called when "trace on next user interaction" is enabled, to notify the user
   * that the tracer is initialized but waiting for the user first input.
   */
  onTracingPending() {
    // Delegate to JavaScriptTracer to log to stdout
    return true;
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
    // Delegate to JavaScriptTracer to log to stdout
    return true;
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

    // By returning true, we let JavaScriptTracer class log the message to stdout.
    return true;
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

    // By returning true, we let JavaScriptTracer class log the message to stdout.
    return true;
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

    // By returning true, we let JavaScriptTracer class log the message to stdout.
    return true;
  }
}

exports.StdoutTracingListener = StdoutTracingListener;
