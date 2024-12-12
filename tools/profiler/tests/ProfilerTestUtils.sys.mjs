/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Assert } from "resource://testing-common/Assert.sys.mjs";
import { StructuredLogger } from "resource://testing-common/StructuredLog.sys.mjs";

/*
 * This module implements useful utilites for interacting with the profiler,
 * as well as querying profiles captured during tests.
 */
export var ProfilerTestUtils = {
  // The marker phases.
  markerPhases: {
    INSTANT: 0,
    INTERVAL: 1,
    INTERVAL_START: 2,
    INTERVAL_END: 3,
  },

  /**
   * This is a helper function to start the profiler with a settings object,
   * while additionally performing checks to ensure that the profiler is not
   * already running when we call this function.
   *
   * @param {Object} callersSettings The settings object to deconstruct and pass
   *   to the profiler. Unspecified settings are overwritten by the default:
   *   {
   *     entries: 8 * 1024 * 1024
   *     interval: 1
   *     features: []
   *     threads: ["GeckoMain"]
   *     activeTabId: 0
   *     duration: 0
   *   }
   * @returns {Promise} The promise returned by StartProfiler. This will resolve
   *   once all child processes have started their own profiler.
   */
  async startProfiler(callersSettings) {
    const defaultSettings = {
      entries: 8 * 1024 * 1024, // 8M entries = 64MB
      interval: 1, // ms
      features: [],
      threads: ["GeckoMain"],
      activeTabId: 0,
      duration: 0,
    };
    if (Services.profiler.IsActive()) {
      Assert.ok(
        Services.env.exists("MOZ_PROFILER_STARTUP"),
        "The profiler is active at the begining of the test, " +
          "the MOZ_PROFILER_STARTUP environment variable should be set."
      );
      if (Services.env.exists("MOZ_PROFILER_STARTUP")) {
        // If the startup profiling environment variable exists, it is likely
        // that tests are being profiled.
        // Stop the profiler before starting profiler tests.
        StructuredLogger.info(
          "This test starts and stops the profiler and is not compatible " +
            "with the use of MOZ_PROFILER_STARTUP. " +
            "Stopping the profiler before starting the test."
        );
        await Services.profiler.StopProfiler();
      } else {
        throw new Error(
          "The profiler must not be active before starting it in a test."
        );
      }
    }
    const settings = Object.assign({}, defaultSettings, callersSettings);
    return Services.profiler.StartProfiler(
      settings.entries,
      settings.interval,
      settings.features,
      settings.threads,
      settings.activeTabId,
      settings.duration
    );
  },

  /**
   * This is a helper function to start the profiler for marker tests, and is
   * just a wrapper around `startProfiler` with some specific defaults.
   */
  async startProfilerForMarkerTests() {
    return this.startProfiler({
      features: ["nostacksampling", "js"],
      threads: ["GeckoMain", "DOM Worker"],
    });
  },

  /**
   * Get the payloads of a type recursively, including from all subprocesses.
   *
   * @param {Object} profile The gecko profile.
   * @param {string} type The marker payload type, e.g. "DiskIO".
   * @param {Array} payloadTarget The recursive list of payloads.
   * @return {Array} The final payloads.
   */
  getPayloadsOfTypeFromAllThreads(profile, type, payloadTarget = []) {
    for (const { markers } of profile.threads) {
      for (const markerTuple of markers.data) {
        const payload = markerTuple[markers.schema.data];
        if (payload && payload.type === type) {
          payloadTarget.push(payload);
        }
      }
    }

    for (const subProcess of profile.processes) {
      this.getPayloadsOfTypeFromAllThreads(subProcess, type, payloadTarget);
    }

    return payloadTarget;
  },

  /**
   * Get the payloads of a type from a single thread.
   *
   * @param {Object} thread The thread from a profile.
   * @param {string} type The marker payload type, e.g. "DiskIO".
   * @return {Array} The payloads.
   */
  getPayloadsOfType(thread, type) {
    const { markers } = thread;
    const results = [];
    for (const markerTuple of markers.data) {
      const payload = markerTuple[markers.schema.data];
      if (payload && payload.type === type) {
        results.push(payload);
      }
    }
    return results;
  },

  /**
   * Applies the marker schema to create individual objects for each marker
   *
   * @param {Object} thread The thread from a profile.
   * @return {InflatedMarker[]} The markers.
   */
  getInflatedMarkerData(thread) {
    const { markers, stringTable } = thread;
    return markers.data.map(markerTuple => {
      const marker = {};
      for (const [key, tupleIndex] of Object.entries(markers.schema)) {
        marker[key] = markerTuple[tupleIndex];
        if (key === "name") {
          // Use the string from the string table.
          marker[key] = stringTable[marker[key]];
        }
      }
      return marker;
    });
  },

  /**
   * Applies the marker schema to create individual objects for each marker, then
   * keeps only the network markers that match the profiler tests.
   *
   * @param {Object} thread The thread from a profile.
   * @return {InflatedMarker[]} The filtered network markers.
   */
  getInflatedNetworkMarkers(thread) {
    const markers = this.getInflatedMarkerData(thread);
    return markers.filter(
      m =>
        m.data &&
        m.data.type === "Network" &&
        // We filter out network markers that aren't related to the test, to
        // avoid intermittents.
        m.data.URI.includes("/tools/profiler/")
    );
  },

  /**
   * From a list of network markers, this returns pairs of start/stop markers.
   * If a stop marker can't be found for a start marker, this will return an array
   * of only 1 element.
   *
   * @param {InflatedMarker[]} networkMarkers Network markers
   * @return {InflatedMarker[][]} Pairs of network markers
   */
  getPairsOfNetworkMarkers(allNetworkMarkers) {
    // For each 'start' marker we want to find the next 'stop' or 'redirect'
    // marker with the same id.
    const result = [];
    const mapOfStartMarkers = new Map(); // marker id -> id in result array
    for (const marker of allNetworkMarkers) {
      const { data } = marker;
      if (data.status === "STATUS_START") {
        if (mapOfStartMarkers.has(data.id)) {
          const previousMarker = result[mapOfStartMarkers.get(data.id)][0];
          Assert.ok(
            false,
            `We found 2 start markers with the same id ${data.id}, without end marker in-between.` +
              `The first marker has URI ${previousMarker.data.URI}, the second marker has URI ${data.URI}.` +
              ` This should not happen.`
          );
          continue;
        }

        mapOfStartMarkers.set(data.id, result.length);
        result.push([marker]);
      } else {
        // STOP or REDIRECT
        if (!mapOfStartMarkers.has(data.id)) {
          Assert.ok(
            false,
            `We found an end marker without a start marker (id: ${data.id}, URI: ${data.URI}). This should not happen.`
          );
          continue;
        }
        result[mapOfStartMarkers.get(data.id)].push(marker);
        mapOfStartMarkers.delete(data.id);
      }
    }

    return result;
  },

  /**
   * It can be helpful to force the profiler to collect a JavaScript sample. This
   * function spins on a while loop until at least one more sample is collected.
   *
   * @return {number} The index of the collected sample.
   */
  captureAtLeastOneJsSample() {
    function getProfileSampleCount() {
      const profile = Services.profiler.getProfileData();
      return profile.threads[0].samples.data.length;
    }

    const sampleCount = getProfileSampleCount();
    // Create an infinite loop until a sample has been collected.
    // eslint-disable-next-line no-constant-condition
    while (true) {
      if (sampleCount < getProfileSampleCount()) {
        return sampleCount;
      }
    }
  },

  /**
   * Verify that a given JSON string is compact - i.e. does not contain
   * unexpected whitespace.
   *
   * @param {String} the JSON string to check
   * @return {Bool} Whether the string is compact or not
   */
  verifyJSONStringIsCompact(s) {
    function isJSONWhitespace(c) {
      return ["\n", "\r", " ", "\t"].includes(c);
    }
    const stateData = 0;
    const stateString = 1;
    const stateEscapedChar = 2;
    let state = stateData;
    for (let i = 0; i < s.length; ++i) {
      let c = s[i];
      switch (state) {
        case stateData:
          if (isJSONWhitespace(c)) {
            Assert.ok(
              false,
              `"Unexpected JSON whitespace at index ${i} in profile: <<<${s}>>>"`
            );
            return;
          }
          if (c == '"') {
            state = stateString;
          }
          break;
        case stateString:
          if (c == '"') {
            state = stateData;
          } else if (c == "\\") {
            state = stateEscapedChar;
          }
          break;
        case stateEscapedChar:
          state = stateString;
          break;
      }
    }
  },

  /**
   * This function pauses the profiler before getting the profile. Then after
   * getting the data, the profiler is stopped, and all profiler data is removed.
   * @returns {Promise<Profile>}
   */
  async stopNowAndGetProfile() {
    // Don't await the pause, because each process will handle it before it
    // receives the following `getProfileDataAsArrayBuffer()`.
    Services.profiler.Pause();

    const profileArrayBuffer =
      await Services.profiler.getProfileDataAsArrayBuffer();
    await Services.profiler.StopProfiler();

    const profileUint8Array = new Uint8Array(profileArrayBuffer);
    const textDecoder = new TextDecoder("utf-8", { fatal: true });
    const profileString = textDecoder.decode(profileUint8Array);
    this.verifyJSONStringIsCompact(profileString);

    return JSON.parse(profileString);
  },

  /**
   * This function ensures there's at least one sample, then pauses the profiler
   * before getting the profile. Then after getting the data, the profiler is
   * stopped, and all profiler data is removed.
   * @returns {Promise<Profile>}
   */
  async waitSamplingAndStopAndGetProfile() {
    await Services.profiler.waitOnePeriodicSampling();
    return this.stopNowAndGetProfile();
  },

  /**
   * Verifies that a marker is an interval marker.
   *
   * @param {InflatedMarker} marker
   * @returns {boolean}
   */
  isIntervalMarker(inflatedMarker) {
    return (
      inflatedMarker.phase === 1 &&
      typeof inflatedMarker.startTime === "number" &&
      typeof inflatedMarker.endTime === "number"
    );
  },

  /**
   * @param {Profile} profile
   * @returns {Thread[]}
   */
  getThreads(profile) {
    const threads = [];

    function getThreadsRecursive(process) {
      for (const thread of process.threads) {
        threads.push(thread);
      }
      for (const subprocess of process.processes) {
        getThreadsRecursive(subprocess);
      }
    }

    getThreadsRecursive(profile);
    return threads;
  },

  /**
   * Find a specific marker schema from any process of a profile.
   *
   * @param {Profile} profile
   * @param {string} name
   * @returns {MarkerSchema}
   */
  getSchema(profile, name) {
    {
      const schema = profile.meta.markerSchema.find(s => s.name === name);
      if (schema) {
        return schema;
      }
    }
    for (const subprocess of profile.processes) {
      const schema = subprocess.meta.markerSchema.find(s => s.name === name);
      if (schema) {
        return schema;
      }
    }
    console.error("Parent process schema", profile.meta.markerSchema);
    for (const subprocess of profile.processes) {
      console.error("Child process schema", subprocess.meta.markerSchema);
    }
    throw new Error(`Could not find a schema for "${name}".`);
  },
};
