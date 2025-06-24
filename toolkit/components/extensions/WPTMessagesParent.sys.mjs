/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = XPCOMUtils.declareLazy({
  actorMatches: {
    pref: "extensions.wpt.matches",
    default: "*://web-platform.test/*",
  },
});

// Map events with positional params to WPT messages with named properties.
const EVENTS = {
  "test-task-start": "test-started testName",
  "test-task-done": "test-finished remainingTests",
  "test-eq": "assert-equality result message expectedValue actualValue",
  "test-result": "assert result message",
};

// Routes extension test results to web-platform.test child actors.
export class WPTMessagesParent extends JSWindowActorParent {
  static init(apiManager) {
    this.apiManager = apiManager;
    apiManager.on("startup", this.#onStartup);
    apiManager.on("shutdown", this.#onShutdown);

    ChromeUtils.registerWindowActor("WPTMessages", {
      matches: lazy.actorMatches.split(","),
      allFrames: true,
      child: {
        esModuleURI: "resource://gre/modules/WPTMessagesChild.sys.mjs",
        observers: ["content-document-global-created"],
      },
      parent: {
        esModuleURI: "resource://gre/modules/WPTMessagesParent.sys.mjs",
      },
    });
  }

  static unload() {
    this.apiManager.off("startup", this.#onStartup);
    this.apiManager.off("shutdown", this.#onShutdown);
    ChromeUtils.unregisterWindowActor("WPTMessages");
  }

  receiveMessage({ name }) {
    switch (name) {
      case "onMessage.addListener":
        WPTMessagesParent.#setListener(this);
        break;
      case "onMessage.removeListener":
        WPTMessagesParent.#removeListener(this);
        break;
    }
  }

  didDestroy() {
    WPTMessagesParent.#removeListener(this);
  }

  static #setListener(actor) {
    // The latest actor to subscribe becomes the only listener.
    this.#listener = actor;
    this.#queue.forEach(args => this.#emit(...args));
    this.#queue.length = 0;
  }

  static #removeListener(actor) {
    // Only clear the listener if the next one hasn't already subscribed.
    if (this.#listener === actor) {
      this.#listener = null;
    }
  }

  static #onStartup(_, ext) {
    for (let ev of Object.keys(EVENTS)) {
      ext.on(ev, WPTMessagesParent.#onEvent);
    }
  }

  static #onShutdown(_, ext) {
    for (let ev of Object.keys(EVENTS)) {
      ext.off(ev, WPTMessagesParent.#onEvent);
    }
  }

  static #onEvent(event, ...args) {
    let [ev, ...keys] = EVENTS[event].split(" ");
    let data = keys.map((key, i) => [key, args[i]]);
    WPTMessagesParent.#emit(ev, Object.fromEntries(data));
  }

  static #emit(event, data) {
    if (this.#listener) {
      return this.#listener.sendAsyncMessage(event, data);
    }
    // Queue messages sent while we don't have a listener.
    this.#queue.push([event, data]);
  }

  // One listener at a time for reliable queing.
  static #listener = null;
  static #queue = [];
}
