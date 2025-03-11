/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WindowGlobalBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/WindowGlobalBiDiModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

/**
 * Internal module to set the configuration on the newly created navigables.
 */
class _ConfigurationModule extends WindowGlobalBiDiModule {
  #preloadScripts;
  #resolveBlockerPromise;
  #viewportConfiguration;

  constructor(messageHandler) {
    super(messageHandler);

    this.#preloadScripts = new Set();
    this.#viewportConfiguration = new Map();

    Services.obs.addObserver(this, "document-element-inserted");
  }

  destroy() {
    // Unblock the document parsing.
    if (this.#resolveBlockerPromise) {
      this.#resolveBlockerPromise();
    }

    Services.obs.removeObserver(this, "document-element-inserted");

    this.#preloadScripts = null;
    this.#viewportConfiguration = null;
  }

  async observe(subject, topic) {
    if (topic === "document-element-inserted") {
      const window = subject?.defaultView;
      // Ignore events without a window.
      if (window !== this.messageHandler.window) {
        return;
      }

      // Do nothing if there is no configuration to apply.
      if (
        this.#preloadScripts.size === 0 &&
        this.#viewportConfiguration.size === 0
      ) {
        return;
      }

      // Block document parsing.
      const blockerPromise = new Promise(resolve => {
        this.#resolveBlockerPromise = resolve;
      });
      this.messageHandler.window.document.blockParsing(blockerPromise);

      if (this.#viewportConfiguration.size !== 0) {
        await this.messageHandler.forwardCommand({
          moduleName: "browsingContext",
          commandName: "_updateNavigableViewport",
          destination: {
            type: lazy.RootMessageHandler.type,
          },
          params: {
            navigable: this.messageHandler.context,
            viewportOverride: Object.fromEntries(this.#viewportConfiguration),
          },
        });
      }

      if (this.#preloadScripts.size !== 0) {
        await this.messageHandler.handleCommand({
          moduleName: "script",
          commandName: "_evaluatePreloadScripts",
          destination: {
            type: lazy.WindowGlobalMessageHandler.type,
            id: this.messageHandler.context.id,
          },
          params: {
            scripts: this.#preloadScripts,
          },
        });
      }

      // Continue script parsing.
      this.#resolveBlockerPromise();
    }
  }

  /**
   * Internal commands
   */

  _applySessionData(params) {
    const { category, sessionData } = params;

    if (category === "preload-script") {
      this.#preloadScripts.clear();

      for (const { contextDescriptor, value } of sessionData) {
        if (!this.messageHandler.matchesContext(contextDescriptor)) {
          continue;
        }

        this.#preloadScripts.add(value);
      }
    }

    // Viewport overrides apply only to top-level traversables.
    if (
      category === "viewport-overrides" &&
      !this.messageHandler.context.parent
    ) {
      for (const { contextDescriptor, value } of sessionData) {
        if (!this.messageHandler.matchesContext(contextDescriptor)) {
          continue;
        }

        if (value.viewport !== undefined) {
          this.#viewportConfiguration.set("viewport", value.viewport);
        }

        if (value.devicePixelRatio !== undefined) {
          this.#viewportConfiguration.set(
            "devicePixelRatio",
            value.devicePixelRatio
          );
        }
      }
    }
  }
}

export const _configuration = _ConfigurationModule;
