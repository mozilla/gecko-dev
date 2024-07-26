/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  RootMessageHandler:
    "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs",
});

class NetworkModule extends Module {
  destroy() {}

  interceptEvent(name, payload) {
    if (name == "network._beforeStopRequest") {
      const { channelId, decodedBodySize } = payload;
      this.messageHandler.handleCommand({
        moduleName: "network",
        commandName: "_setDecodedBodySize",
        params: { channelId, decodedBodySize },
        destination: {
          type: lazy.RootMessageHandler.type,
        },
      });
    }

    // The _beforeStopRequest event is only used in order to feed the
    // decodedBodySize map in the parent process. Return null to swallow the
    // event.
    return null;
  }
}

export const network = NetworkModule;
