/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1898946 - UA spoof for transcrib.io
 *
 * This checks navigator.userAgent and navigator.vendor to detect Chrome,
 * so let's set appropriate values to also look like Chrome.
 */

/* globals exportFunction */

console.info(
  "The user agent and navigator.vendor have been shimmed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1898946 for details."
);

const CHROME_UA = navigator.userAgent + " Chrome for WebCompat";

Object.defineProperty(window.navigator.wrappedJSObject, "userAgent", {
  get: exportFunction(function () {
    return CHROME_UA;
  }, window),

  set: exportFunction(function () {}, window),
});

Object.defineProperty(window.navigator.wrappedJSObject, "vendor", {
  get: exportFunction(function () {
    return "Google Inc.";
  }, window),

  set: exportFunction(function () {}, window),
});
