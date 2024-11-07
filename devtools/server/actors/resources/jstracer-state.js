/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { JSTracer } = ChromeUtils.importESModule(
  "resource://devtools/server/tracer/tracer.sys.mjs",
  { global: "contextual" }
);

const Targets = require("resource://devtools/server/actors/targets/index.js");
loader.lazyRequireGetter(
  this,
  "TRACER_LOG_METHODS",
  "resource://devtools/shared/specs/tracer.js",
  true
);

class TracingStateWatcher {
  /**
   * Start watching for tracing state changes for a given target actor.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory function
   *          This will be called for each resource.
   */
  async watch(targetActor, { onAvailable }) {
    // Bug 1874204: tracer doesn't support tracing content process from the browser toolbox just yet
    if (targetActor.targetType == Targets.TYPES.PROCESS) {
      return;
    }

    this.targetActor = targetActor;
    this.onAvailable = onAvailable;

    this.tracingListener = {
      onTracingToggled: this.onTracingToggled.bind(this),
    };
    JSTracer.addTracingListener(this.tracingListener);
  }

  /**
   * Stop watching for tracing state
   */
  destroy() {
    if (!this.tracingListener) {
      return;
    }
    JSTracer.removeTracingListener(this.tracingListener);
  }

  /**
   * Be notified by the underlying JavaScriptTracer class
   * in case it stops by itself, instead of being stopped when the Actor's stopTracing
   * method is called by the user.
   *
   * @param {Boolean} enabled
   *        True if the tracer starts tracing, false it it stops.
   * @param {String} reason
   *        Optional string to justify why the tracer stopped.
   */
  async onTracingToggled(enabled, reason) {
    const tracerActor = this.targetActor.getTargetScopedActor("tracer");
    const logMethod = tracerActor?.getLogMethod();

    // JavascriptTracer only supports recording once in the same process/thread.
    // If we open another DevTools, on the same process, we would receive notification
    // about a JavascriptTracer controlled by another toolbox's tracer actor.
    // Ignore them as our current tracer actor didn't start tracing.
    if (!logMethod) {
      return;
    }

    this.onAvailable([
      {
        enabled,
        logMethod,
        profile:
          logMethod == TRACER_LOG_METHODS.PROFILER && !enabled
            ? await tracerActor.getProfile()
            : undefined,
        timeStamp: ChromeUtils.dateNow(),
        reason,
        traceValues: tracerActor.traceValues,
      },
    ]);
  }
}

module.exports = TracingStateWatcher;
