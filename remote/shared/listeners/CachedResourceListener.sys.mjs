/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EventEmitter: "resource://gre/modules/EventEmitter.sys.mjs",
  NetworkUtils:
    "resource://devtools/shared/network-observer/NetworkUtils.sys.mjs",
});

const OBSERVER_TOPIC_RESOURCE_CACHE_RESPONSE =
  "http-on-resource-cache-response";

/**
 * The CachedResourceListener can be used to listen for
 * http-on-resource-cache-response notifications emitted for resources
 * which are served by the image/CSS/JS cache.
 * This notification needs to be monitored in content processes.
 *
 * Example:
 * ```
 * const listener = new CachedResourceListener();
 * listener.on("cached-resource-sent", CachedResourceListener);
 * listener.startListening();
 *
 * const onCachedResourceSent = (eventName, data = {}) => {
 *   const { channel } = data;
 *   ...
 * };
 * ```
 *
 * @fires cached-resource-sent
 *    The CachedResourceListener emits "cached-resource-sent" event with the
 *    following object as payload:
 *      - {nsIHttpChannel} channel
 *            The channel for which the observer notification was emitted.
 */

export class CachedResourceListener {
  #context;
  #listening;

  /**
   * Create a new CachedResourceListener instance.
   *
   * @param {BrowsingContext} context
   *     The browsing context to filter the events for.
   */
  constructor(context) {
    lazy.EventEmitter.decorate(this);

    this.#listening = false;
    this.#context = context;
  }

  destroy() {
    this.stopListening();
  }

  observe(subject, topic) {
    switch (topic) {
      case OBSERVER_TOPIC_RESOURCE_CACHE_RESPONSE: {
        // file channels are not supported:
        //   https://bugzilla.mozilla.org/show_bug.cgi?id=1826210
        if (!(subject instanceof Ci.nsIHttpChannel)) {
          return;
        }
        const channel = subject.QueryInterface(Ci.nsIHttpChannel);
        const id = lazy.NetworkUtils.getChannelBrowsingContextID(channel);
        const browsingContext = BrowsingContext.get(id);

        // Send the event only if the notification comes for observed context.
        if (this.#context === browsingContext) {
          this.emit("cached-resource-sent", {
            channel,
          });
        }
        break;
      }
    }
  }

  startListening() {
    if (this.#listening) {
      return;
    }

    Services.obs.addObserver(this, OBSERVER_TOPIC_RESOURCE_CACHE_RESPONSE);

    this.#listening = true;
  }

  stopListening() {
    if (!this.#listening) {
      return;
    }

    Services.obs.removeObserver(this, OBSERVER_TOPIC_RESOURCE_CACHE_RESPONSE);

    this.#listening = false;
  }
}
