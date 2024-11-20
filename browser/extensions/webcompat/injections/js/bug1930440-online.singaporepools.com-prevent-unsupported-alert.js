/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals exportFunction */

"use strict";

/**
 * online.singaporepools.com - Shows an 'unsupported browser' alert
 * Bug #1930440 - https://bugzilla.mozilla.org/show_bug.cgi?id=1930440
 * WebCompat issue #143685 - https://webcompat.com/issues/143685
 *
 * We can intercept the call to alert and hide it.
 */

console.info(
  "window.alert is being overriden for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1923286 for details."
);

const originalAlert = Object.getOwnPropertyDescriptor(
  window.wrappedJSObject,
  "alert"
).value;

Object.defineProperty(window.wrappedJSObject, "alert", {
  get: exportFunction(function (msg) {
    if (!msg.includes("unsupported browser")) {
      originalAlert.apply(this, arguments);
    }
  }, window),

  set: exportFunction(function () {}, window),
});
