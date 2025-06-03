/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  DeferredTask: "resource://gre/modules/DeferredTask.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "MAX_SUBMISSION_DELAY_PREF_VALUE",
  "browser.newtabpage.activity-stream.telemetry.privatePing.maxSubmissionDelayMs",
  5000
);

export class NewTabContentPing {
  #eventBuffer = [];
  #deferredTask = null;
  #lastDelaySelection = 0;

  /**
   * Adds a event recording for Glean.newtabContent to the internal buffer.
   * The event will be recorded when the ping is sent.
   *
   * @param {string} name
   *   The name of the event to record.
   * @param {object} data
   *   The extra data being recorded with the event.
   */
  recordEvent(name, data) {
    this.#eventBuffer.push([name, this.sanitizeEventData(data)]);
  }

  /**
   * Schedules the sending of the newtab-content ping at some randomly selected
   * point in the future.
   *
   * @param {object} privateMetrics
   *   The metrics to send along with the ping when it is sent, keyed on the
   *   name of the metric.
   */
  scheduleSubmission(privateMetrics) {
    for (let metric of Object.keys(privateMetrics)) {
      try {
        Glean.newtabContent[metric].set(privateMetrics[metric]);
      } catch (e) {
        console.error(e);
      }
    }

    if (!this.#deferredTask) {
      this.#lastDelaySelection = this.#generateRandomSubmissionDelayMs();
      this.#deferredTask = new lazy.DeferredTask(() => {
        this.#flushEventsAndSubmit();
      }, this.#lastDelaySelection);
      this.#deferredTask.arm();
    }
  }

  /**
   * Disarms any pre-existing scheduled newtab-content pings and clears the
   * event buffer.
   */
  uninit() {
    this.#deferredTask?.disarm();
    this.#eventBuffer = [];
  }

  /**
   * Called by the DeferredTask when the randomly selected delay has elapsed
   * after calling scheduleSubmission.
   */
  #flushEventsAndSubmit() {
    this.#deferredTask = null;

    let events = this.#eventBuffer;
    this.#eventBuffer = [];

    for (let [eventName, data] of events) {
      try {
        Glean.newtabContent[eventName].record(data);
      } catch (e) {
        console.error(e);
      }
    }

    GleanPings.newtabContent.submit();
  }

  /**
   * Removes fields from an event that can be linked to a user in any way, in
   * order to preserve anonymity of the newtab_content ping. This is just to
   * ensure we don't accidentally send these if copying information between
   * the newtab ping and the newtab-content ping.
   *
   * @param {object} eventDataDict
   *   The Glean event data that would be passed to a `record` method.
   * @returns {object}
   *   The sanitized event data.
   */
  sanitizeEventData(eventDataDict) {
    const {
      // eslint-disable-next-line no-unused-vars
      tile_id,
      // eslint-disable-next-line no-unused-vars
      newtab_visit_id,
      // eslint-disable-next-line no-unused-vars
      matches_selected_topic,
      // eslint-disable-next-line no-unused-vars
      recommended_at,
      // eslint-disable-next-line no-unused-vars
      received_rank,
      // eslint-disable-next-line no-unused-vars
      event_source,
      ...result
    } = eventDataDict;
    return result;
  }

  /**
   * Generate a random delay to submit the ping from the point of
   * scheduling. This uses a cryptographically secure mechanism for
   * generating the random delay and returns it in millseconds.
   *
   * @returns {number}
   *   A random number between 1000 and the max new content ping submission
   *   delay pref.
   */
  #generateRandomSubmissionDelayMs() {
    const MIN_SUBMISSION_DELAY = 1000;

    if (lazy.MAX_SUBMISSION_DELAY_PREF_VALUE <= MIN_SUBMISSION_DELAY) {
      // Somehow we got configured with a maximum delay less than the minimum...
      // Let's fallback to 5000 then.
      console.error(
        "Can not have a newtab-content maximum submission delay less" +
          ` than 1000: ${lazy.MAX_SUBMISSION_DELAY_PREF_VALUE}`
      );
    }
    const MAX_SUBMISSION_DELAY =
      lazy.MAX_SUBMISSION_DELAY_PREF_VALUE > MIN_SUBMISSION_DELAY
        ? lazy.MAX_SUBMISSION_DELAY_PREF_VALUE
        : 5000;

    const RANGE = MAX_SUBMISSION_DELAY - MIN_SUBMISSION_DELAY + 1;
    const MAX_UINT32 = 0xffffffff;

    // To ensure a uniform distribution, we discard values that could introduce
    // modulo bias. We divide the 2^32 range into equal-sized "buckets" and only
    // accept random values that fall entirely within one of these buckets.
    // This ensures each possible output in the target range is equally likely.
    const BUCKET_SIZE = Math.floor(MAX_UINT32 / RANGE);
    const MAX_ACCEPTABLE = BUCKET_SIZE * RANGE;

    let selection;
    let randomValues = new Uint32Array(1);

    do {
      crypto.getRandomValues(randomValues);
      [selection] = randomValues;
    } while (selection >= MAX_ACCEPTABLE);

    return MIN_SUBMISSION_DELAY + (selection % RANGE);
  }

  /**
   * This is a test-only function that will disarm the DeferredTask from sending
   * the newtab-content ping, and instead send it manually. The originally
   * selected submission delay is returned.
   *
   * This function is a no-op when not running in test automation.
   *
   * @returns {number}
   *   The originally selected random delay for submitting the newtab-content
   *   ping.
   * @throws {Error}
   *   Throws if this is called when no submission has been scheduled yet.
   */
  testOnlyForceFlush() {
    if (!Cu.isInAutomation) {
      return 0;
    }

    if (this.#deferredTask) {
      this.#deferredTask.disarm();
      this.#deferredTask = null;
      this.#flushEventsAndSubmit();
      return this.#lastDelaySelection;
    }
    throw new Error("No submission was scheduled.");
  }
}
