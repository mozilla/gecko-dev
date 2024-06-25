/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint-disable no-unused-vars */

// The fallback color for unexpected cases
const DEFAULT_COLOR = "grey";

// The default category for unexpected cases
const DEFAULT_CATEGORIES = [
  {
    name: "Mixed",
    color: DEFAULT_COLOR,
    subcategories: ["Other"],
  },
];

// Color for each type of category/frame's implementation
const PREDEFINED_COLORS = {
  interpreter: "yellow",
  baseline: "orange",
  ion: "blue",
  wasm: "purple",

  label: "lightblue",
};

// Indexes of attributes in arrays
const INDEXES = {
  stacks: {
    prefix: 0,
    frame: 1,
  },
  frames: {
    location: 0,
    relevantForJS: 1,
    innerWindowID: 2,
    implementation: 3,
    line: 4,
    column: 5,
    category: 6,
    subcategory: 7,
  },
};

class ProfilerTracingListener {
  constructor({ targetActor, traceValues, traceActor }) {
    this.targetActor = targetActor;
    this.traceValues = traceValues;
    this.sourcesManager = targetActor.sourcesManager;
    this.traceActor = traceActor;

    this.#reset();
    this.#thread = this.#getEmptyThread();
    this.#startTime = ChromeUtils.dateNow();
  }

  #thread = null;
  #stackMap = new Map();
  #frameMap = new Map();
  #categories = DEFAULT_CATEGORIES;
  #currentStackIndex = null;
  #startTime = null;

  /**
   * Stop the record and return the gecko profiler data.
   *
   * @return {Object}
   *         The Gecko profile object.
   */
  stop() {
    // Create the profile to return.
    const profile = this.#getEmptyProfile();
    profile.meta.categories = this.#categories;
    profile.threads.push(this.#thread);

    // Cleanup.
    this.#reset();

    return profile;
  }

  /**
   * Clear all the internal state of this class.
   */
  #reset() {
    this.#thread = null;
    this.#stackMap = new Map();
    this.#frameMap = new Map();
    this.#categories = DEFAULT_CATEGORIES;
    this.#currentStackIndex = null;
  }

  /**
   * Initialize an empty Gecko profile object.
   *
   * @return {Object}
   *         Gecko profile object.
   */
  #getEmptyProfile() {
    const httpHandler = isWorker
      ? {}
      : Cc["@mozilla.org/network/protocol;1?name=http"].getService(
          Ci.nsIHttpProtocolHandler
        );
    return {
      meta: {
        // Currently interval is 1, but the frontend mostly ignores this (mandatory) attribute.
        // Instead it relies on sample's 'time' attribute to position frames in the stack chart.
        interval: 1,
        startTime: this.#startTime,
        product: Services.appinfo?.name,
        importedFrom: "JS Tracer",
        version: 28,
        presymbolicated: true,
        abi: Services.appinfo?.XPCOMABI,
        misc: httpHandler.misc,
        oscpu: httpHandler.oscpu,
        platform: httpHandler.platform,
        processType: Services.appinfo?.processType,
        categories: [],
        stackwalk: 0,
        toolkit: Services.appinfo?.widgetToolkit,
        appBuildID: Services.appinfo?.appBuildID,
        sourceURL: Services.appinfo?.sourceURL,
        physicalCPUs: 0,
        logicalCPUs: 0,
        CPUName: "",
        markerSchema: [],
      },
      libs: [],
      pages: [],
      threads: [],
      processes: [],
    };
  }

  /**
   * Generate a thread object to be stored in the Gecko profile object.
   */
  #getEmptyThread() {
    return {
      processType: "default",
      processStartupTime: 0,
      processShutdownTime: null,
      registerTime: 0,
      unregisterTime: null,
      pausedRanges: [],
      name: "GeckoMain",
      "eTLD+1": "JS Tracer",
      isMainThread: true,
      // In workers, you wouldn't have access to appinfo
      pid: Services.appinfo?.processID,
      tid: 0,
      samples: {
        schema: {
          stack: 0,
          time: 1,
          eventDelay: 2,
        },
        data: [],
      },
      markers: {
        schema: {
          name: 0,
          startTime: 1,
          endTime: 2,
          phase: 3,
          category: 4,
          data: 5,
        },
        data: [],
      },
      stackTable: {
        schema: INDEXES.stacks,
        data: [],
      },
      frameTable: {
        schema: INDEXES.frames,
        data: [],
      },
      stringTable: [],
    };
  }

  /**
   * Get a frame index for a label frame name.
   * Label frame are fake frames in order to display arbitrary strings in the stack chart.
   *
   * @param {String} label
   * @return {Number}
   *         Frame index for this label frame.
   */
  #getOrCreateLabelFrame(label) {
    const { frameTable, stringTable } = this.#thread;
    const key = `label:${label}`;
    let frameIndex = this.#frameMap.get(key);

    if (frameIndex === undefined) {
      frameIndex = frameTable.data.length;
      const locationStringIndex = stringTable.length;

      stringTable.push(label);

      const categoryIndex = this.#getOrCreateCategory("label");

      frameTable.data.push([
        locationStringIndex,
        true, // relevantForJS
        0, // innerWindowID
        null, // implementation
        null, // line
        null, // column
        categoryIndex,
        0, // subcategory
      ]);
      this.#frameMap.set(key, frameIndex);
    }

    return frameIndex;
  }

  /**
   * Get the unique index for the given frame.
   *
   * @param {Object} frameInfo
   * @return {Number}
   *         The index for the given frame.
   */
  #getOrCreateFrame(frameInfo) {
    const { frameTable, stringTable } = this.#thread;
    const key = `${frameInfo.sourceId}:${frameInfo.lineNumber}:${frameInfo.columnNumber}:${frameInfo.category}`;
    let frameIndex = this.#frameMap.get(key);

    if (frameIndex === undefined) {
      frameIndex = frameTable.data.length;
      const locationStringIndex = stringTable.length;

      // Profiler frontend except a particular string to match the source URL:
      // `functionName (http://script.url/:1234:1234)`
      // https://github.com/firefox-devtools/profiler/blob/dab645b2db7e1b21185b286f96dd03b77f68f5c3/src/profile-logic/process-profile.js#L518
      stringTable.push(
        `${frameInfo.name} (${frameInfo.url}:${frameInfo.lineNumber}:${frameInfo.columnNumber})`
      );

      const categoryIndex = this.#getOrCreateCategory(frameInfo.category);

      frameTable.data.push([
        locationStringIndex,
        true, // relevantForJS
        0, // innerWindowID
        null, // implementation
        frameInfo.lineNumber, // line
        frameInfo.columnNumber, // column
        categoryIndex,
        0, // subcategory
      ]);
      this.#frameMap.set(key, frameIndex);
    }

    return frameIndex;
  }

  #getOrCreateStack(frameIndex, prefix) {
    const { stackTable } = this.#thread;
    const key = prefix === null ? `${frameIndex}` : `${frameIndex},${prefix}`;
    let stackIndex = this.#stackMap.get(key);

    if (stackIndex === undefined) {
      stackIndex = stackTable.data.length;
      stackTable.data.push([prefix, frameIndex]);
      this.#stackMap.set(key, stackIndex);
    }
    return stackIndex;
  }

  #getOrCreateCategory(category) {
    const categories = this.#categories;
    let categoryIndex = categories.findIndex(c => c.name === category);

    if (categoryIndex === -1) {
      categoryIndex = categories.length;
      categories.push({
        name: category,
        color: PREDEFINED_COLORS[category] ?? DEFAULT_COLOR,
        subcategories: ["Other"],
      });
    }
    return categoryIndex;
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
    return false;
  }

  onTracingInfiniteLoop() {
    this.traceActor.stopTracing();
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
   */
  onTracingDOMMutation({ depth, prefix, type, caller, element }) {
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
    return false;
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

    if (currentDOMEvent && depth == 0) {
      this.#logDOMEvent(currentDOMEvent);
    }

    const frameIndex = this.#getOrCreateFrame({
      // formatedDisplayName has a lambda at the beginning, remove it.
      name: formatedDisplayName.replace("λ ", ""),
      url,
      lineNumber,
      columnNumber,
      category: frame.implementation,
      sourceId: script.source.id,
    });
    this.#currentStackIndex = this.#getOrCreateStack(
      frameIndex,
      this.#currentStackIndex
    );

    this.#thread.samples.data.push([
      this.#currentStackIndex,
      ChromeUtils.dateNow() - this.#startTime,
      0, // eventDelay
    ]);

    return false;
  }

  /**
   * Called when a DOM Event just fired (and some listener in JS is about to run).
   *
   * @param {String} domEventName
   */
  #logDOMEvent(domEventName) {
    const frameIndex = this.#getOrCreateLabelFrame(domEventName);
    this.#currentStackIndex = this.#getOrCreateStack(
      frameIndex,
      this.#currentStackIndex
    );

    this.#thread.samples.data.push([
      this.#currentStackIndex,
      ChromeUtils.dateNow() - this.#startTime,
      0, // eventDelay
    ]);
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

    this.#onFramePop();

    return false;
  }

  /**
   * Called when a function call ends and returns.
   */
  #onFramePop() {
    if (this.#currentStackIndex === null) {
      return;
    }

    this.#currentStackIndex =
      this.#thread.stackTable.data[this.#currentStackIndex][
        INDEXES.stacks.prefix
      ];

    // Record a sample for the parent's stack (or null if there is none [i.e. on top level frame pop])
    // so that the frontend considers that the last executed frame stops its execution.
    this.#thread.samples.data.push([
      this.#currentStackIndex,
      ChromeUtils.dateNow() - this.#startTime,
      0, // eventDelay
    ]);

    // If we popped and now are on a label frame, with a null line,
    // automatically also pop that label frame.
    if (this.#currentStackIndex !== null) {
      const currentFrameIndex =
        this.#thread.stackTable.data[this.#currentStackIndex][
          INDEXES.stacks.frame
        ];
      const currentFrameLine =
        this.#thread.frameTable.data[currentFrameIndex][INDEXES.frames.line];
      if (currentFrameLine == null) {
        this.#onFramePop();
      }
    }
  }
}

exports.ProfilerTracingListener = ProfilerTracingListener;
