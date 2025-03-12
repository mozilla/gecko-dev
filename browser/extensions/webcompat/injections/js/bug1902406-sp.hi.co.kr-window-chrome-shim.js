/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1902406 - UA spoof for sp.hi.co.kr
 *
 * This site is checking for window.chrome, so let's spoof that.
 */

/* globals exportFunction */

console.info(
  "window.chrome has been shimmed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1902406 for details."
);

Object.defineProperty(window.wrappedJSObject, "chrome", {
  get: exportFunction(function () {
    return true;
  }, window),

  set: exportFunction(function () {}, window),
});
