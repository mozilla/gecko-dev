/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};
ChromeUtils.defineESModuleGetters(
  lazy,
  {
    JSTracer: "resource://devtools/server/tracer/tracer.sys.mjs",
  },
  { global: "contextual" }
);

const { Actor } = require("resource://devtools/shared/protocol.js");
const { createValueGrip } = require("devtools/server/actors/object/utils");
const {
  ObjectActorPool,
} = require("resource://devtools/server/actors/object/ObjectActorPool.js");
const {
  tracerSpec,
  TRACER_LOG_METHODS,
} = require("resource://devtools/shared/specs/tracer.js");

loader.lazyRequireGetter(
  this,
  "StdoutTracingListener",
  "resource://devtools/server/actors/tracer/stdout.js",
  true
);
loader.lazyRequireGetter(
  this,
  "ResourcesTracingListener",
  "resource://devtools/server/actors/tracer/resources.js",
  true
);
loader.lazyRequireGetter(
  this,
  "ProfilerTracingListener",
  "resource://devtools/server/actors/tracer/profiler.js",
  true
);

// Indexes of each data type within the array describing a trace
exports.TRACER_FIELDS_INDEXES = {
  // This is shared with all the data types
  TYPE: 0,

  // Frame traces are slightly special and do not share any field with the other data types
  FRAME_IMPLEMENTATION: 1,
  FRAME_NAME: 2,
  FRAME_SOURCEID: 3,
  FRAME_LINE: 4,
  FRAME_COLUMN: 5,
  FRAME_URL: 6,

  // These fields are shared with all but frame data types
  PREFIX: 1,
  FRAME_INDEX: 2,
  TIMESTAMP: 3,
  DEPTH: 4,

  EVENT_NAME: 5,

  ENTER_ARGS: 5,
  ENTER_ARG_NAMES: 6,

  EXIT_PARENT_FRAME_ID: 5,
  EXIT_RETURNED_VALUE: 6,
  EXIT_WHY: 7,

  DOM_MUTATION_TYPE: 5,
  DOM_MUTATION_ELEMENT: 6,
};

const VALID_LOG_METHODS = Object.values(TRACER_LOG_METHODS);

class TracerActor extends Actor {
  constructor(conn, targetActor) {
    super(conn, tracerSpec);
    this.targetActor = targetActor;
  }

  // When the tracer is stopped, save the result of the Listener Class.
  // This is used by the profiler log method and the getProfile method.
  #stopResult = null;

  // A Pool for all JS values emitted by the Tracer Actor.
  // This helps instantiate a unique Object Actor per JS Object communicated to the client.
  // This also helps share the same Object Actor instances when evaluating JS via
  // the console actor.
  // This pool is created lazily, only once we start a new trace.
  // We also clear the pool before starting the trace.
  #tracerPool = null;

  destroy() {
    this.stopTracing();
  }

  getLogMethod() {
    return this.logMethod;
  }

  /**
   * Toggle tracing JavaScript.
   * Meant for the WebConsole command in order to pass advanced
   * configuration directly to JavaScriptTracer class.
   *
   * @param {Object} options
   *        Options used to configure JavaScriptTracer.
   *        See `JavaScriptTracer.startTracing`.
   * @return {Boolean}
   *         True if the tracer starts, or false if it was stopped.
   */
  toggleTracing(options) {
    if (!this.tracingListener) {
      this.startTracing(options);
      return true;
    }
    this.stopTracing();
    return false;
  }

  /**
   * Start tracing.
   *
   * @param {Object} options
   *        Options used to configure JavaScriptTracer.
   *        See `JavaScriptTracer.startTracing`.
   */
  // eslint-disable-next-line complexity
  startTracing(options = {}) {
    if (options.logMethod && !VALID_LOG_METHODS.includes(options.logMethod)) {
      throw new Error(
        `Invalid log method '${options.logMethod}'. Only supports: ${VALID_LOG_METHODS}`
      );
    }
    if (options.prefix && typeof options.prefix != "string") {
      throw new Error("Invalid prefix, only support string type");
    }
    if (options.maxDepth && typeof options.maxDepth != "number") {
      throw new Error("Invalid max-depth, only support numbers");
    }
    if (options.maxRecords && typeof options.maxRecords != "number") {
      throw new Error("Invalid max-records, only support numbers");
    }

    // When tracing on next user interaction is enabled,
    // disable logging from workers as this makes the tracer work
    // against visible documents and is actived per document thread.
    if (options.traceOnNextInteraction && isWorker) {
      return;
    }

    // Ignore WindowGlobal target actors for WindowGlobal of iframes running in the same process and thread as their parent document.
    // isProcessRoot will be true for each WindowGlobal being the top parent within a given process.
    // It will typically be true for WindowGlobal of iframe running in a distinct origin and process,
    // but only for the top iframe document. It will also be true for the top level tab document.
    if (
      this.targetActor.window &&
      !this.targetActor.window.windowGlobalChild?.isProcessRoot
    ) {
      return;
    }

    // Flush any previous recorded data only when we start a new tracer
    // as we may still analyse trace data after stopping the trace.
    // The pool will then be re-created on demand from createValueGrip.
    if (this.#tracerPool) {
      this.#tracerPool.destroy();
      this.#tracerPool = null;
    }

    this.logMethod = options.logMethod || TRACER_LOG_METHODS.STDOUT;

    let ListenerClass = null;
    // Currently only the profiler output is supported with the native tracer.
    let useNativeTracing = false;
    switch (this.logMethod) {
      case TRACER_LOG_METHODS.STDOUT:
        ListenerClass = StdoutTracingListener;
        break;
      case TRACER_LOG_METHODS.CONSOLE:
      case TRACER_LOG_METHODS.DEBUGGER_SIDEBAR:
        // Console and debugger sidebar are both using JSTRACE_STATE/JSTRACE_TRACE resources
        // to receive tracing data.
        ListenerClass = ResourcesTracingListener;
        break;
      case TRACER_LOG_METHODS.PROFILER:
        ListenerClass = ProfilerTracingListener;
        // Recording function returns is mandatory when recording profiler output.
        // Otherwise frames are not closed and mixed up in the profiler frontend.
        options.traceFunctionReturn = true;
        useNativeTracing = true;
        break;
    }
    this.tracingListener = new ListenerClass({
      targetActor: this.targetActor,
      traceValues: !!options.traceValues,
      traceActor: this,
    });
    lazy.JSTracer.addTracingListener(this.tracingListener);

    this.traceValues = !!options.traceValues;
    try {
      lazy.JSTracer.startTracing({
        global: this.targetActor.targetGlobal,
        prefix: options.prefix || "",
        // Enable receiving the `currentDOMEvent` being passed to `onTracingFrame`
        traceDOMEvents: true,
        // Enable tracing DOM Mutations
        traceDOMMutations: options.traceDOMMutations,
        // Enable tracing function arguments as well as returned values
        traceValues: !!options.traceValues,
        // Enable tracing only on next user interaction
        traceOnNextInteraction: !!options.traceOnNextInteraction,
        // Notify about frame exit / function call returning
        traceFunctionReturn: !!options.traceFunctionReturn,
        // Use the native tracing implementation
        useNativeTracing,
        // Ignore frames beyond the given depth
        maxDepth: options.maxDepth,
        // Stop the tracing after a number of top level frames
        maxRecords: options.maxRecords,
      });
    } catch (e) {
      // If startTracing throws, it probably rejected one of its options and we should
      // unregister the tracing listener.
      this.stopTracing();
      throw e;
    }
  }

  async stopTracing() {
    if (!this.tracingListener) {
      return;
    }
    // Remove before stopping to prevent receiving the stop notification
    lazy.JSTracer.removeTracingListener(this.tracingListener);
    // Save the result of the stop request for the profiler and the getProfile RDP method
    this.#stopResult = this.tracingListener.stop();
    this.tracingListener = null;

    lazy.JSTracer.stopTracing();
    this.logMethod = null;
  }

  /**
   * Queried by THREAD_STATE watcher to send the gecko profiler data
   * as part of THREAD STATE "stop" resource.
   *
   * @return {Object} Gecko profiler profile object.
   */
  async getProfile() {
    // #stopResult is a promise
    return this.#stopResult;
  }

  createValueGrip(value) {
    if (!this.#tracerPool) {
      this.#tracerPool = new ObjectActorPool(
        this.targetActor.threadActor,
        "tracer",
        true
      );
      this.manage(this.#tracerPool);
    }
    return createValueGrip(this, value, this.#tracerPool);
  }
}
exports.TracerActor = TracerActor;
