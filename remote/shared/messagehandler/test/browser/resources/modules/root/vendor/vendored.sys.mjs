/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class VendoredModule extends Module {
  destroy() {}

  /**
   * Commands
   */

  emitVendoredRootEvent() {
    this.emitEvent(
      "vendor:vendored.vendoredRootEvent",
      "vendoredRootEventValue"
    );
  }

  testRoot() {
    return "valueFromRoot";
  }
}

export const vendored = VendoredModule;
