/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * @typedef {import("perf").BulkReceiving} BulkSending
 */

const { Actor } = require("resource://devtools/shared/protocol.js");
const { perfSpec } = require("resource://devtools/shared/specs/perf.js");

ChromeUtils.defineESModuleGetters(
  this,
  {
    RecordingUtils:
      "resource://devtools/shared/performance-new/recording-utils.sys.mjs",
    Symbolication:
      "resource://devtools/shared/performance-new/symbolication.sys.mjs",
  },
  { global: "contextual" }
);

// Some platforms are built without the Gecko Profiler.
const IS_SUPPORTED_PLATFORM = "nsIProfiler" in Ci;

/**
 * The PerfActor wraps the Gecko Profiler interface (aka Services.profiler).
 */
exports.PerfActor = class PerfActor extends Actor {
  /**
   * This counter is incremented at each new capture. This makes sure that the
   * profile data and the additionalInformation are in sync.
   * @type {number}
   */
  #captureHandleCounter = 0;

  /**
   * This stores the profile data retrieved from the last call to
   * startCaptureAndStopProfiler.
   * @type {Promise<ArrayBuffer> |null}
   */
  #previouslyRetrievedProfileDataPromise = null;

  /**
   * This stores the additionalInformation returned by
   * getProfileDataAsGzippedArrayBufferThenStop so that it can be sent to the
   * front using getPreviouslyRetrievedAdditionalInformation.
   * @type {Promise<MockedExports.ProfileGenerationAdditionalInformation>| null}
   */
  #previouslyRetrievedAdditionalInformationPromise = null;

  constructor(conn) {
    super(conn, perfSpec);

    // Only setup the observers on a supported platform.
    if (IS_SUPPORTED_PLATFORM) {
      this._observer = {
        observe: this._observe.bind(this),
      };
      Services.obs.addObserver(this._observer, "profiler-started");
      Services.obs.addObserver(this._observer, "profiler-stopped");
    }
  }

  destroy() {
    super.destroy();

    if (!IS_SUPPORTED_PLATFORM) {
      return;
    }
    Services.obs.removeObserver(this._observer, "profiler-started");
    Services.obs.removeObserver(this._observer, "profiler-stopped");
  }

  startProfiler(options) {
    if (!IS_SUPPORTED_PLATFORM) {
      return false;
    }

    // For a quick implementation, decide on some default values. These may need
    // to be tweaked or made configurable as needed.
    const settings = {
      entries: options.entries || 1000000,
      duration: options.duration || 0,
      interval: options.interval || 1,
      features: options.features || ["js", "stackwalk", "cpu", "memory"],
      threads: options.threads || ["GeckoMain", "Compositor"],
      activeTabID: RecordingUtils.getActiveBrowserID(),
    };

    try {
      // This can throw an error if the profiler is in the wrong state.
      Services.profiler.StartProfiler(
        settings.entries,
        settings.interval,
        settings.features,
        settings.threads,
        settings.activeTabID,
        settings.duration
      );
    } catch (e) {
      // In case any errors get triggered, bailout with a false.
      return false;
    }

    return true;
  }

  stopProfilerAndDiscardProfile() {
    if (!IS_SUPPORTED_PLATFORM) {
      return null;
    }
    return Services.profiler.StopProfiler();
  }

  /**
   * @type {string} debugPath
   * @type {string} breakpadId
   * @returns {Promise<[number[], number[], number[]]>}
   */
  async getSymbolTable(debugPath, breakpadId) {
    const libraries = Services.profiler.sharedLibraries;
    const symbolicationService = Symbolication.createLocalSymbolicationService(
      libraries,
      []
    );
    const debugName = libraries.find(
      lib => lib.path === debugPath && lib.breakpadId === breakpadId
    )?.debugName;

    if (debugName === undefined) {
      throw new Error(
        `Couldn't find the library with path ${debugPath} and breakpadId ${breakpadId}`
      );
    }

    const [addr, index, buffer] = await symbolicationService.getSymbolTable(
      debugName,
      breakpadId
    );
    // The protocol does not support the transfer of typed arrays, so we convert
    // these typed arrays to plain JS arrays of numbers now.
    // Our return value type is declared as "array:array:number".
    return [Array.from(addr), Array.from(index), Array.from(buffer)];
  }

  /* @backward-compat { version 140 }
   * Version 140 introduced getProfileAndStopProfilerBulk below, a more
   * efficient version of getProfileAndStopProfiler. getProfileAndStopProfiler
   * needs to stay in the spec to support older versions of Firefox, so it's
   * also present here. */
  async getProfileAndStopProfiler() {
    throw new Error(
      "Unexpected getProfileAndStopProfiler function called in Firefox v140+. Most likely you're using an older version of Firefox to debug this application. Please use at least Firefox v140."
    );
  }

  async startCaptureAndStopProfiler() {
    if (!IS_SUPPORTED_PLATFORM) {
      throw new Error("Profiling is not supported on this platform.");
    }

    const capturePromise =
      RecordingUtils.getProfileDataAsGzippedArrayBufferThenStop();

    this.#previouslyRetrievedProfileDataPromise = capturePromise.then(
      ({ profileCaptureResult }) => {
        if (profileCaptureResult.type === "ERROR") {
          throw profileCaptureResult.error;
        }

        return profileCaptureResult.profile;
      }
    );

    this.#previouslyRetrievedAdditionalInformationPromise = capturePromise.then(
      ({ additionalInformation }) => additionalInformation
    );

    return ++this.#captureHandleCounter;
  }

  /**
   * This actor function returns the profile data using the bulk protocol.
   * @param {number} handle returned by startCaptureAndStopProfiler
   * @returns {Promise<void>}
   */
  async getPreviouslyCapturedProfileDataBulk(handle, startBulkSend) {
    if (handle < this.#captureHandleCounter) {
      // This handle is outdated, write a message to the console and throw an error
      console.error(
        `[devtools perf actor] In getPreviouslyCapturedProfileDataBulk, the requested handle ${handle} is smaller than the current counter ${this.#captureHandleCounter}.`
      );
      throw new Error(`The requested data was not found.`);
    }

    if (this.#previouslyRetrievedProfileDataPromise === null) {
      // No capture operation has been started, write a message and throw an error.
      console.error(
        `[devtools perf actor] In getPreviouslyCapturedProfileDataBulk, there's no data to be returned.`
      );
      throw new Error(`The requested data was not found.`);
    }

    // Note that this promise might be rejected if there was an error. That's OK
    // and part of the design.
    const profile = await this.#previouslyRetrievedProfileDataPromise;
    this.#previouslyRetrievedProfileDataPromise = null;

    const bulk = await startBulkSend(profile.byteLength);
    await bulk.copyFromBuffer(profile);
  }

  /**
   * @param {number} handle returned by startCaptureAndStopProfiler
   * @returns {Promise<MockedExports.ProfileGenerationAdditionalInformation>}
   */
  async getPreviouslyRetrievedAdditionalInformation(handle) {
    if (handle < this.#captureHandleCounter) {
      // This handle is outdated, write a message to the console and throw an error
      console.error(
        `[devtools perf actor] In getPreviouslyRetrievedAdditionalInformation, the requested handle ${handle} is smaller than the current counter ${this.#captureHandleCounter}.`
      );
      throw new Error(`The requested data was not found.`);
    }

    if (this.#previouslyRetrievedAdditionalInformationPromise === null) {
      // No capture operation has been started, write a message and throw an error.
      console.error(
        `[devtools perf actor] In getPreviouslyRetrievedAdditionalInformation, there's no data to be returned.`
      );
      throw new Error(`The requested data was not found.`);
    }

    try {
      return this.#previouslyRetrievedAdditionalInformationPromise;
    } finally {
      this.#previouslyRetrievedAdditionalInformationPromise = null;
    }
  }

  isActive() {
    if (!IS_SUPPORTED_PLATFORM) {
      return false;
    }
    return Services.profiler.IsActive();
  }

  isSupportedPlatform() {
    return IS_SUPPORTED_PLATFORM;
  }

  /**
   * Watch for events that happen within the browser. These can affect the
   * current availability and state of the Gecko Profiler.
   */
  _observe(subject, topic, _data) {
    // Note! If emitting new events make sure and update the list of bridged
    // events in the perf actor.
    switch (topic) {
      case "profiler-started": {
        const param = subject.QueryInterface(Ci.nsIProfilerStartParams);
        this.emit(
          topic,
          param.entries,
          param.interval,
          param.features,
          param.duration,
          param.activeTabID
        );
        break;
      }
      case "profiler-stopped":
        this.emit(topic);
        break;
    }
  }

  /**
   * Lists the supported features of the profiler for the current browser.
   * @returns {string[]}
   */
  getSupportedFeatures() {
    if (!IS_SUPPORTED_PLATFORM) {
      return [];
    }
    return Services.profiler.GetFeatures();
  }
};
