/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1902379 - UA spoof for adl.edu.tw
 *
 * This site is checking for Chrome in navigator.userAgent and vendor, so let's spoof those.
 */

/* globals exportFunction, UAHelpers */

console.info(
  "navigator.userAgent and navigator.platform are being shimmed for compatibility reasons. https://bugzilla.mozilla.org/show_bug.cgi?id=1902379 for details."
);

const CHROME_UA = UAHelpers.addChrome();

const nav = Object.getPrototypeOf(navigator.wrappedJSObject);

const ua = Object.getOwnPropertyDescriptor(nav, "userAgent");
ua.get = exportFunction(() => CHROME_UA, window);
Object.defineProperty(nav, "userAgent", ua);

const vendor = Object.getOwnPropertyDescriptor(nav, "vendor");
vendor.get = exportFunction(() => "Google Inc.", window);
Object.defineProperty(nav, "vendor", vendor);
