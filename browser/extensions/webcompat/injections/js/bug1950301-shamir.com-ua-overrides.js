/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1950301 - UA spoofs for shamir.com
 *
 * Something about the site's Rocket loader code breaks when the useragent is Firefox and
 * there is no navigator.vendor set. We can spoof these values to bypass the breakage.
 */

/* globals exportFunction */

console.info(
  "navigator.userAgent and navigator.vendor have been shimmed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1950301 for details."
);

const UA = navigator.userAgent.replace("Firefox", "FireFox");

Object.defineProperty(window.navigator.wrappedJSObject, "userAgent", {
  get: exportFunction(function () {
    return UA;
  }, window),

  set: exportFunction(function () {}, window),
});

Object.defineProperty(window.navigator.wrappedJSObject, "vendor", {
  get: exportFunction(function () {
    return "Mozilla";
  }, window),

  set: exportFunction(function () {}, window),
});
