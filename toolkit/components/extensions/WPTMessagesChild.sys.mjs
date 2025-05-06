/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.crashIfNotInAutomation();

// Exposes the browser.test.onMessage event on web-platform.test pages.
export class WPTMessagesChild extends JSWindowActorChild {
  observe() {
    // Only observing "content-document-global-created" to trigger the actor.
  }

  actorCreated() {
    let win = this.contentWindow.wrappedJSObject;
    if (!Object.hasOwn(win, "browser")) {
      let test = {
        onMessage: {
          addListener: arg => this.#addListener(arg),
          removeListener: arg => this.#removeListener(arg),
        },
      };
      Object.defineProperty(win, "browser", {
        configurable: true,
        enumerable: true,
        value: Cu.cloneInto({ test }, win, { cloneFunctions: true }),
        writable: true,
      });
    }
  }

  receiveMessage({ name, data }) {
    for (let callback of this.#listeners) {
      try {
        callback(name, Cu.cloneInto(data, this.contentWindow));
      } catch (e) {
        Cu.reportError(
          `Error in browser.test.onMessage listener: ${e.message}`,
          e.stack
        );
      }
    }
  }

  #addListener(callback) {
    if (!this.#listeners.size) {
      this.sendAsyncMessage("onMessage.addListener");
    }
    this.#listeners.add(callback);
  }

  #removeListener(callback) {
    this.#listeners.delete(callback);
    if (!this.#listeners.size) {
      this.sendAsyncMessage("onMessage.removeListener");
    }
  }

  #listeners = new Set();
}
