/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1957657 - myheritage.com email signup field does not work on Android
 *
 * The site does a call to blur an element at an unexpected time, and hits
 * a probable interop bug in Firefox. We can ignore that specific blur call.
 */

/* globals exportFunction */

console.info(
  "scrollToTerms has been overridden for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1957657 for details."
);

Object.defineProperty(window.wrappedJSObject, "scrollToTerms", {
  writable: false,
  value: exportFunction(function () {
    document.querySelector("#miniSignupForm")?.scrollIntoView();
  }, window),
});
