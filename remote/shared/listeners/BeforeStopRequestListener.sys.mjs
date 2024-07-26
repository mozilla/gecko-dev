/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.sys.mjs",
});

const OBSERVER_TOPIC_BEFORE_STOP_REQUEST = "http-on-before-stop-request";

/**
 * The BeforeStopRequestListener can be used to listen for
 * http-on-before-stop-request notifications emitted right before a network
 * channel is stopped. At this point the response should be completely decoded
 * and the channel decodedBodySize property should have the expected value.
 * This notification needs to be monitored in content processes, because
 * decodedBodySize is always set to 0 in the parent process.
 *
 * Example:
 * ```
 * const listener = new BeforeStopRequestListener();
 * listener.on("beforeStopRequest", onBeforeStopRequest);
 * listener.startListening();
 *
 * const onBeforeStopRequest = (eventName, data = {}) => {
 *   const { channel, decodedBodySize } = data;
 *   ...
 * };
 * ```
 *
 * @fires message
 *    The BeforeStopRequestListener emits "beforeStopRequest" events with the
 *    following object as payload:
 *      - {nsIHttpChannel} channel
 *            The channel for which the observer notification was emitted.
 *      - {number} decodedBodySize
 *            The decoded body size for the channel.
 */
export class BeforeStopRequestListener {
  #listening;

  /**
   * Create a new BeforeStopRequestListener instance.
   */
  constructor() {
    lazy.EventEmitter.decorate(this);

    this.#listening = false;
  }

  destroy() {
    this.stopListening();
  }

  observe(subject, topic) {
    switch (topic) {
      case OBSERVER_TOPIC_BEFORE_STOP_REQUEST: {
        const channel = subject.QueryInterface(Ci.nsIHttpChannel);
        this.emit("beforeStopRequest", {
          channel,
          decodedBodySize: channel.decodedBodySize,
        });
        break;
      }
    }
  }

  startListening() {
    if (this.#listening) {
      return;
    }

    Services.obs.addObserver(this, OBSERVER_TOPIC_BEFORE_STOP_REQUEST);

    this.#listening = true;
  }

  stopListening() {
    if (!this.#listening) {
      return;
    }

    Services.obs.removeObserver(this, OBSERVER_TOPIC_BEFORE_STOP_REQUEST);

    this.#listening = false;
  }
}
