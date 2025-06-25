/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

try {
  const { BrowserLoader } = ChromeUtils.importESModule(
    "resource://devtools/shared/loader/browser-loader.sys.mjs"
  );
  // Expose both `browserLoader` and `Debugger` to panel.js
  globalThis.browserLoader = BrowserLoader({
    baseURI: "resource://devtools/client/debugger",
    window,
  });
  globalThis.Debugger = globalThis.browserLoader.require(
    "devtools/client/debugger/src/main"
  );
  // Expose `require` for the CustomFormatter ESM in order to allow it to load
  // ObjectInspector, which are still CommonJS modules, via the same BrowserLoader instance.
  globalThis.browserLoaderRequire = globalThis.browserLoader.require;
} catch (e) {
  dump("Exception happened while loading the debugger:\n");
  dump(e + "\n");
  dump(e.stack + "\n");
}
