/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WindowGlobalBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/WindowGlobalBiDiModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BeforeStopRequestListener:
    "chrome://remote/content/shared/listeners/BeforeStopRequestListener.sys.mjs",
});

class NetworkModule extends WindowGlobalBiDiModule {
  #beforeStopRequestListener;
  #subscribedEvents;

  constructor(messageHandler) {
    super(messageHandler);

    this.#beforeStopRequestListener = new lazy.BeforeStopRequestListener();
    this.#beforeStopRequestListener.on(
      "beforeStopRequest",
      this.#onBeforeStopRequest
    );

    // Set of event names which have active subscriptions.
    this.#subscribedEvents = new Set();
  }

  destroy() {
    this.#beforeStopRequestListener.destroy();
    this.#subscribedEvents = null;
  }

  #onBeforeStopRequest = (event, data) => {
    this.messageHandler.emitEvent("network._beforeStopRequest", {
      channelId: data.channel.channelId,
      contextId: this.messageHandler.contextId,
      decodedBodySize: data.decodedBodySize,
    });
  };

  #startListening() {
    if (this.#subscribedEvents.size == 0) {
      this.#beforeStopRequestListener.startListening();
    }
  }

  #stopListening() {
    if (this.#subscribedEvents.size == 0) {
      this.#beforeStopRequestListener.stopListening();
    }
  }

  #subscribeEvent(event) {
    switch (event) {
      case "network.responseCompleted":
        this.#startListening();
        this.#subscribedEvents.add("network.responseCompleted");
        break;
    }
  }

  #unsubscribeEvent(event) {
    switch (event) {
      case "network.responseCompleted":
        this.#subscribedEvents.delete("network.responseCompleted");
        break;
    }

    this.#stopListening();
  }

  /**
   * Internal commands
   */

  _applySessionData(params) {
    // TODO: Bug 1775231. Move this logic to a shared module or an abstract
    // class.
    const { category } = params;
    if (category === "event") {
      const filteredSessionData = params.sessionData.filter(item =>
        this.messageHandler.matchesContext(item.contextDescriptor)
      );
      for (const event of this.#subscribedEvents.values()) {
        const hasSessionItem = filteredSessionData.some(
          item => item.value === event
        );
        // If there are no session items for this context, we should unsubscribe from the event.
        if (!hasSessionItem) {
          this.#unsubscribeEvent(event);
        }
      }

      // Subscribe to all events, which have an item in SessionData.
      for (const { value } of filteredSessionData) {
        this.#subscribeEvent(value);
      }
    }
  }
}

export const network = NetworkModule;
