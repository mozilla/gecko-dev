/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");
const Services = require("Services");
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils.js");

let DEFAULT_PROFILER_OPTIONS = {
  // When using the DevTools Performance Tools, this will be overridden
  // by the pref `devtools.performance.profiler.buffer-size`.
  entries: Math.pow(10, 7),
  // When using the DevTools Performance Tools, this will be overridden
  // by the pref `devtools.performance.profiler.sample-rate-khz`.
  interval: 1,
  features: ["js"],
  threadFilters: ["GeckoMain"]
};

/**
 * The nsIProfiler is target agnostic and interacts with the whole platform.
 * Therefore, special care needs to be given to make sure different actor
 * consumers (i.e. "toolboxes") don't interfere with each other.
 */
let gProfilerConsumers = 0;

loader.lazyGetter(this, "nsIProfilerModule", () => {
  return Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
});

/**
 * The profiler actor provides remote access to the built-in nsIProfiler module.
 */
function ProfilerActor() {
  gProfilerConsumers++;
  this._observedEvents = new Set();
}

ProfilerActor.prototype = {
  actorPrefix: "profiler",
  disconnect: function() {
    for (let event of this._observedEvents) {
      Services.obs.removeObserver(this, event);
    }
    this._observedEvents = null;
    this.onStopProfiler();

    gProfilerConsumers--;
    checkProfilerConsumers();
  },

  /**
   * Returns an array of feature strings, describing the profiler features
   * that are available on this platform. Can be called while the profiler
   * is stopped.
   */
  onGetFeatures: function() {
    return { features: nsIProfilerModule.GetFeatures([]) };
  },

  /**
   * Returns an object with the values of the current status of the
   * circular buffer in the profiler, returning `position`, `totalSize`,
   * and the current `generation` of the buffer.
   */
  onGetBufferInfo: function() {
    let position = {}, totalSize = {}, generation = {};
    nsIProfilerModule.GetBufferInfo(position, totalSize, generation);
    return {
      position: position.value,
      totalSize: totalSize.value,
      generation: generation.value
    }
  },

  /**
   * Returns the configuration used that was originally passed in to start up the
   * profiler. Used for tests, and does not account for others using nsIProfiler.
   */
  onGetStartOptions: function() {
    return this._profilerStartOptions || {};
  },

  /**
   * Starts the nsIProfiler module. Doing so will discard any samples
   * that might have been accumulated so far.
   *
   * @param number entries [optional]
   * @param number interval [optional]
   * @param array:string features [optional]
   * @param array:string threadFilters [description]
   */
  onStartProfiler: function(request = {}) {
    let options = this._profilerStartOptions = {
      entries: request.entries || DEFAULT_PROFILER_OPTIONS.entries,
      interval: request.interval || DEFAULT_PROFILER_OPTIONS.interval,
      features: request.features || DEFAULT_PROFILER_OPTIONS.features,
      threadFilters: request.threadFilters || DEFAULT_PROFILER_OPTIONS.threadFilters,
    };

    // The start time should be before any samples we might be
    // interested in.
    let currentTime = nsIProfilerModule.getElapsedTime();

    nsIProfilerModule.StartProfiler(
      options.entries,
      options.interval,
      options.features,
      options.features.length,
      options.threadFilters,
      options.threadFilters.length
    );
    let { position, totalSize, generation } = this.onGetBufferInfo();

    return { started: true, position, totalSize, generation, currentTime };
  },

  /**
   * Stops the nsIProfiler module, if no other client is using it.
   */
  onStopProfiler: function() {
    // Actually stop the profiler only if the last client has stopped profiling.
    // Since this is a root actor, and the profiler module interacts with the
    // whole platform, we need to avoid a case in which the profiler is stopped
    // when there might be other clients still profiling.
    if (gProfilerConsumers == 1) {
      nsIProfilerModule.StopProfiler();
    }
    return { started: false };
  },

  /**
   * Verifies whether or not the nsIProfiler module has started.
   * If already active, the current time is also returned.
   */
  onIsActive: function() {
    let isActive = nsIProfilerModule.IsActive();
    let elapsedTime = isActive ? nsIProfilerModule.getElapsedTime() : undefined;
    let { position, totalSize, generation } = this.onGetBufferInfo();
    return { isActive: isActive, currentTime: elapsedTime, position, totalSize, generation };
  },

  /**
   * Returns a stringified JSON object that describes the shared libraries
   * which are currently loaded into our process. Can be called while the
   * profiler is stopped.
   */
  onGetSharedLibraryInformation: function() {
    return { sharedLibraryInformation: nsIProfilerModule.getSharedLibraryInformation() };
  },

  /**
   * Returns all the samples accumulated since the profiler was started,
   * along with the current time. The data has the following format:
   * {
   *   libs: string,
   *   meta: {
   *     interval: number,
   *     platform: string,
   *     ...
   *   },
   *   threads: [{
   *     samples: [{
   *       frames: [{
   *         line: number,
   *         location: string,
   *         category: number
   *       } ... ],
   *       name: string
   *       responsiveness: number
   *       time: number
   *     } ... ]
   *   } ... ]
   * }
   *
   *
   * @param number startTime
   *        Since the circular buffer will only grow as long as the profiler lives,
   *        the buffer can contain unwanted samples. Pass in a `startTime` to only retrieve
   *        samples that took place after the `startTime`, with 0 being when the profiler
   *        just started.
   */
  onGetProfile: function(request) {
    let startTime = request.startTime || 0;
    let profile = nsIProfilerModule.getProfileData(startTime);
    return { profile: profile, currentTime: nsIProfilerModule.getElapsedTime() };
  },

  /**
   * Registers for certain event notifications.
   * Currently supported events:
   *   - "console-api-profiler"
   *   - "profiler-started"
   *   - "profiler-stopped"
   */
  onRegisterEventNotifications: function(request) {
    let response = [];
    for (let event of request.events) {
      if (this._observedEvents.has(event)) {
        continue;
      }
      Services.obs.addObserver(this, event, false);
      this._observedEvents.add(event);
      response.push(event);
    }
    return { registered: response };
  },

  /**
   * Unregisters from certain event notifications.
   * Currently supported events:
   *   - "console-api-profiler"
   *   - "profiler-started"
   *   - "profiler-stopped"
   */
  onUnregisterEventNotifications: function(request) {
    let response = [];
    for (let event of request.events) {
      if (!this._observedEvents.has(event)) {
        continue;
      }
      Services.obs.removeObserver(this, event);
      this._observedEvents.delete(event);
      response.push(event);
    }
    return { unregistered: response };
  },

  /**
   * Callback for all observed notifications.
   * @param object subject
   * @param string topic
   * @param object data
   */
  observe: DevToolsUtils.makeInfallible(function(subject, topic, data) {
    // Create JSON objects suitable for transportation across the RDP,
    // by breaking cycles and making a copy of the `subject` and `data` via
    // JSON.stringifying those values with a replacer that omits properties
    // known to introduce cycles, and then JSON.parsing the result.
    // This spends some CPU cycles, but it's simple.
    subject = (subject && !Cu.isXrayWrapper(subject) && subject.wrappedJSObject) || subject;
    subject = JSON.parse(JSON.stringify(subject, cycleBreaker));
    data = (data && !Cu.isXrayWrapper(data) && data.wrappedJSObject) || data;
    data = JSON.parse(JSON.stringify(data, cycleBreaker));

    // Sends actor, type and other additional information over the remote
    // debugging protocol to any profiler clients.
    let reply = details => {
      this.conn.send({
        from: this.actorID,
        type: "eventNotification",
        subject: subject,
        topic: topic,
        data: data,
        details: details
      });
    };

    switch (topic) {
      case "console-api-profiler":
        return void reply(this._handleConsoleEvent(subject, data));
      case "profiler-started":
      case "profiler-stopped":
      default:
        return void reply();
    }
  }, "ProfilerActor.prototype.observe"),

  /**
   * Handles `console.profile` and `console.profileEnd` invocations and
   * creates an appropriate response sent over the protocol.
   * @param object subject
   * @param object data
   * @return object
   */
  _handleConsoleEvent: function(subject, data) {
    // An optional label may be specified when calling `console.profile`.
    // If that's the case, stringify it and send it over with the response.
    let { action, arguments: args } = subject;
    let profileLabel = args.length > 0 ? args[0] + "" : undefined;

    // If the event was generated from `console.profile` or `console.profileEnd`
    // we need to start the profiler right away and then just notify the client.
    // Otherwise, we'll lose precious samples.

    if (action === "profile" || action === "profileEnd") {
      let { isActive, currentTime } = this.onIsActive();

      // Start the profiler only if it wasn't already active. Otherwise, any
      // samples that might have been accumulated so far will be discarded.
      if (!isActive && action === "profile") {
        this.onStartProfiler();
        return {
          profileLabel: profileLabel,
          currentTime: 0
        };
      }
      // Otherwise, if inactive and a call to profile end, send
      // an empty object because we can't do anything with this.
      else if (!isActive) {
        return {};
      }

      // Otherwise, the profiler is already active, so just send
      // to the front the current time, label, and the notification
      // adds the action as well.
      return {
        profileLabel: profileLabel,
        currentTime: currentTime
      };
    }
  }
};

exports.ProfilerActor = ProfilerActor;

/**
 * JSON.stringify callback used in ProfilerActor.prototype.observe.
 */
function cycleBreaker(key, value) {
  if (key == "wrappedJSObject") {
    return undefined;
  }
  return value;
}

/**
 * Asserts the value sanity of `gProfilerConsumers`.
 */
function checkProfilerConsumers() {
  if (gProfilerConsumers < 0) {
    let msg = "Somehow the number of started profilers is now negative.";
    DevToolsUtils.reportException("ProfilerActor", msg);
  }
}

/**
 * The request types this actor can handle.
 * At the moment there are two known users of the Profiler actor:
 * the devtools and the Gecko Profiler addon, which uses the debugger
 * protocol to get profiles from Fennec.
 */
ProfilerActor.prototype.requestTypes = {
  "getBufferInfo": ProfilerActor.prototype.onGetBufferInfo,
  "getFeatures": ProfilerActor.prototype.onGetFeatures,
  "startProfiler": ProfilerActor.prototype.onStartProfiler,
  "stopProfiler": ProfilerActor.prototype.onStopProfiler,
  "isActive": ProfilerActor.prototype.onIsActive,
  "getSharedLibraryInformation": ProfilerActor.prototype.onGetSharedLibraryInformation,
  "getProfile": ProfilerActor.prototype.onGetProfile,
  "registerEventNotifications": ProfilerActor.prototype.onRegisterEventNotifications,
  "unregisterEventNotifications": ProfilerActor.prototype.onUnregisterEventNotifications,
  "getStartOptions": ProfilerActor.prototype.onGetStartOptions
};
