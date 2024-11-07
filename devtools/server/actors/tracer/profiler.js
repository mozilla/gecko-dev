/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// The functions in the class use standard functions called from tracer.js but we want to keep the
// arguments intact.
/* eslint "no-unused-vars": ["error", {args: "none"} ]*/

class ProfilerTracingListener {
  constructor({ targetActor, traceActor }) {
    this.targetActor = targetActor;
    this.traceActor = traceActor;
  }

  /**
   * Stop the record and return the gecko profiler data.
   *
   * @param {Object} nativeTrace
   *         If we're using native tracing, this contains a table of what the
   *         native tracer has collected.
   * @return {Object}
   *         The Gecko profile object.
   */
  async stop(nativeTrace) {
    // Pause profiler before we collect the profile, so that we don't capture
    // more samples while the parent process or android threads wait for subprocess profiles.
    Services.profiler.Pause();

    let profile;
    try {
      // Attempt to pull out the data.
      profile = await Services.profiler.getProfileDataAsync();

      if (Object.keys(profile).length === 0) {
        console.error(
          "An empty object was received from getProfileDataAsync.getProfileDataAsync(), " +
            "meaning that a profile could not successfully be serialized and captured."
        );
        profile = null;
      }
    } catch (e) {
      // Explicitly set the profile to null if there as an error.
      profile = null;
      console.error(`There was an error fetching a profile`, e);
    }

    Services.profiler.StopProfiler();

    return profile;
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
    } else {
      Services.profiler.StartProfiler(
        // Note that this is the same default as profiler ones defined in:
        // devtools/client/performance-new/shared/background.sys.mjs
        128 * 1024 * 1024,
        1,
        ["screenshots", "tracing"],
        ["GeckoMain", "DOM Worker"],
        this.targetActor.sessionContext.browserId,
        0
      );
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
    let elementDescription = element.tagName?.toLowerCase();
    if (element.id) {
      elementDescription += `#${element.id}`;
    }
    if (element.className) {
      elementDescription += `.${element.className.trim().replace(/ +/g, ".")}`;
    }

    const description = `${type} on ${elementDescription}`;

    // Bug 1904602: we need a tweak in profiler frontend before being able to show
    // dom mutation in the stack chart. Until then, add a custom marker.
    ChromeUtils.addProfilerMarker("DOM-Mutation", undefined, description);

    return false;
  }
}

exports.ProfilerTracingListener = ProfilerTracingListener;
