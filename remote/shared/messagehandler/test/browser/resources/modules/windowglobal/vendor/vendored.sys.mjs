/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class VendoredModule extends Module {
  #sessionDataValue;

  constructor(messageHandler) {
    super(messageHandler);
    this.#sessionDataValue = null;
  }
  destroy() {}

  /**
   * Commands
   */

  _applySessionData(params) {
    if (params.category === "vendored-session-data") {
      this.#sessionDataValue = params.sessionData;
    }
  }

  emitVendoredWindowGlobalEvent() {
    this.emitEvent(
      "vendor:vendored.vendoredWindowGlobalEvent",
      "vendoredWindowGlobalEventValue"
    );
  }

  getSessionDataValue() {
    return this.#sessionDataValue;
  }

  testWindowGlobal() {
    return "valueFromWindowGlobal";
  }
}

export const vendored = VendoredModule;
