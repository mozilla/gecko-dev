/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

try {
  const { BrowserLoader } = ChromeUtils.importESModule(
    "resource://devtools/shared/loader/browser-loader.sys.mjs"
  );
  const { require } = BrowserLoader({
    baseURI: "resource://devtools/client/debugger",
    window,
  });
  globalThis.Debugger = require("devtools/client/debugger/src/main");
} catch (e) {
  dump("Exception happened while loading the debugger:\n");
  dump(e + "\n");
  dump(e.stack + "\n");
}
