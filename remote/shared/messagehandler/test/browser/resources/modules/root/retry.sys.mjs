/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.sys.mjs",
});

// The test is supposed to trigger the command and then destroy the
// JSWindowActor pair by any mean (eg a navigation) in order to trigger an
// AbortError and a retry.
class RetryModule extends Module {
  destroy() {}

  /**
   * Commands
   */

  async waitForDiscardedBrowsingContext(params = {}) {
    const { browsingContext, retryOnAbort } = params;

    // Wait for the browsing context to be discarded (replaced or destroyed)
    // before calling the internal command.
    await new Promise(resolve => {
      const observe = (_subject, _topic, _data) => {
        Services.obs.removeObserver(observe, "browsing-context-discarded");
        resolve();
      };
      Services.obs.addObserver(observe, "browsing-context-discarded");
    });

    return this.messageHandler.forwardCommand({
      moduleName: "retry",
      commandName: "_internalForward",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: browsingContext.id,
      },
      retryOnAbort,
    });
  }
}

export const retry = RetryModule;
