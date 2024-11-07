/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");

loader.lazyRequireGetter(
  this,
  "TRACER_LOG_METHODS",
  "resource://devtools/shared/specs/tracer.js",
  true
);

class TracerCommand extends EventEmitter {
  constructor({ commands }) {
    super();
    this.#targetConfigurationCommand = commands.targetConfigurationCommand;
    this.#resourceCommand = commands.resourceCommand;
  }

  // The tracer has been requested to start, but doesn't necessarily mean it actually started tracing JS executions.
  // The tracer may wait for next user interaction/document load before being active.
  isTracingEnabled = false;
  // The tracer is actively tracking JS executions.
  isTracingActive = false;

  #resourceCommand;
  #targetConfigurationCommand;

  async initialize() {
    return this.#resourceCommand.watchResources(
      [this.#resourceCommand.TYPES.JSTRACER_STATE],
      { onAvailable: this.onResourcesAvailable }
    );
  }

  destroy() {
    this.#resourceCommand.unwatchResources(
      [this.#resourceCommand.TYPES.JSTRACER_STATE],
      { onAvailable: this.onResourcesAvailable }
    );
  }

  onResourcesAvailable = resources => {
    for (const resource of resources) {
      if (resource.resourceType != this.#resourceCommand.TYPES.JSTRACER_STATE) {
        continue;
      }

      // Clear the list of collected frames each time we start a new tracer record.
      // The tracer will reset its frame counter to zero on stop, but on the frontend
      // we may inspect frames after the tracer is stopped, until we start a new one.
      if (resource.enabled) {
        resource.targetFront.getJsTracerCollectedFramesArray().length = 0;
      }

      if (
        resource.enabled == this.isTracingActive &&
        resource.enabled == this.isTracingEnabled
      ) {
        continue;
      }

      this.isTracingActive = resource.enabled;
      // In case the tracer is started without the DevTools frontend, also force it to be reported as enabled
      this.isTracingEnabled = resource.enabled;

      this.emit("toggle");
    }
  };

  /**
   * Get the dictionary passed to the server codebase as a SessionData.
   * This contains all settings to fine tune the tracer actual behavior.
   *
   * @return {JSON}
   *         Configuration object.
   */
  getTracingOptions() {
    const logMethod = Services.prefs.getStringPref(
      "devtools.debugger.javascript-tracing-log-method",
      ""
    );
    return {
      logMethod,
      // Force enabling DOM Mutation logging as soon as we selected the sidebar log output
      traceDOMMutations:
        logMethod == TRACER_LOG_METHODS.DEBUGGER_SIDEBAR ||
        logMethod == TRACER_LOG_METHODS.PROFILER
          ? ["add", "attributes", "remove"]
          : null,
      traceValues: Services.prefs.getBoolPref(
        "devtools.debugger.javascript-tracing-values",
        false
      ),
      traceOnNextInteraction: Services.prefs.getBoolPref(
        "devtools.debugger.javascript-tracing-on-next-interaction",
        false
      ),
      traceOnNextLoad: Services.prefs.getBoolPref(
        "devtools.debugger.javascript-tracing-on-next-load",
        false
      ),
      traceFunctionReturn: Services.prefs.getBoolPref(
        "devtools.debugger.javascript-tracing-function-return",
        false
      ),
    };
  }

  /**
   * Toggle JavaScript tracing for all targets.
   */
  async toggle() {
    this.isTracingEnabled = !this.isTracingEnabled;

    // May be wait for the web console to be fully initialized and listening to tracer resources before enabling it
    await this.emitAsync("toggle");

    await this.#targetConfigurationCommand.updateConfiguration({
      tracerOptions: this.isTracingEnabled
        ? this.getTracingOptions()
        : undefined,
    });
  }
}

module.exports = TracerCommand;
