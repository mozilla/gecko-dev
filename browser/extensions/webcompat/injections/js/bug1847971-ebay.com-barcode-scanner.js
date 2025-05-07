/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1847971 - ebay.com listings page
 *
 * eBay does not show the barcode scanner option on Firefox on Android,
 * though it seems to work. It uses a server-side check to tell itself
 * to not enable the scanner feature, so rather than pretend to be Chrome
 * entirely, we carefully change `barcodeEnabled` from `false` to `true`.
 */

/* globals exportFunction */

const ENABLED_MESSAGE =
  "The barcode scanner feature has been force-enabled. See https://bugzilla.mozilla.org/show_bug.cgi?id=1847971 for details.";

let value = undefined;

function check(obj) {
  if (obj === null) {
    return false;
  }
  if (Array.isArray(obj)) {
    for (const v of obj) {
      if (check(v)) {
        return true;
      }
    }
    return false;
  }
  if (typeof obj === "object") {
    if ("barcodeEnabled" in obj) {
      obj.barcodeEnabled = true;
      return true;
    }
    for (const v of Object.values(obj)) {
      if (check(v)) {
        return true;
      }
    }
  }
  return false;
}

Object.defineProperty(window.wrappedJSObject, "$feprelist_C", {
  configurable: true,

  get: exportFunction(function () {
    return value;
  }, window),

  set: exportFunction(function (v) {
    try {
      if (check(v)) {
        console.info(ENABLED_MESSAGE);
      }
    } catch (_) {}
    value = v;
  }, window),
});
