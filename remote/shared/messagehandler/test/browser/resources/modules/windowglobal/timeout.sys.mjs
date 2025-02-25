/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";
import { setTimeout } from "resource://gre/modules/Timer.sys.mjs";

class TimeoutModule extends Module {
  destroy() {}

  blockProcess(params) {
    const start = Date.now();
    while (Date.now() - start < params.delay) {
      // Do nothing
    }
  }

  async waitFor(params) {
    await new Promise(r => setTimeout(r, params.delay));
    return true;
  }
}

export const timeout = TimeoutModule;
