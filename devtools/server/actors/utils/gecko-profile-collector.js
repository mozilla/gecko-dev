/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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
};

/**
 * Utility class that collects the JS tracer data and converts it to a Gecko
 * profile object.
 */
class GeckoProfileCollector {
  #thread = null;
  #stackMap = new Map();
  #frameMap = new Map();
  #categories = DEFAULT_CATEGORIES;
  #currentStackIndex = null;
  #startTime = null;

  /**
   * Initialize the profiler and be ready to receive samples.
   */
  start() {
    this.#reset();
    this.#thread = this.#getEmptyThread();
    this.#startTime = ChromeUtils.dateNow();
  }

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
        schema: {
          prefix: 0,
          frame: 1,
        },
        data: [],
      },
      frameTable: {
        schema: {
          location: 0,
          relevantForJS: 1,
          innerWindowID: 2,
          implementation: 3,
          line: 4,
          column: 5,
          category: 6,
          subcategory: 7,
        },
        data: [],
      },
      stringTable: [],
    };
  }

  /**
   * Called when a new function is called.
   *
   * @param {Object} frameInfo
   */
  onEnterFrame(frameInfo) {
    const frameIndex = this.#getOrCreateFrame(frameInfo);
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
   * Called when a function call ends and returns.
   */
  onFramePop() {
    if (this.#currentStackIndex === null) {
      return;
    }

    this.#currentStackIndex =
      this.#thread.stackTable.data[this.#currentStackIndex][0];

    // Record a sample for the parent's stack (or null if there is none [i.e. on top level frame pop])
    // so that the frontend considers that the last executed frame stops its execution.
    this.#thread.samples.data.push([
      this.#currentStackIndex,
      ChromeUtils.dateNow() - this.#startTime,
      0, // eventDelay
    ]);
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
}

exports.GeckoProfileCollector = GeckoProfileCollector;
