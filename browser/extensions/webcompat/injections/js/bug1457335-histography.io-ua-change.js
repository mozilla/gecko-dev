/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1457335 - histography.io - Override UA & navigator.vendor
 * WebCompat issue #1804 - https://webcompat.com/issues/1804
 *
 * This site is using a strict matching of navigator.userAgent and
 * navigator.vendor to allow access for Safari or Chrome. Here, we set the
 * values appropriately so we get recognized as Chrome.
 */

/* globals exportFunction */

console.info(
  "The user agent has been overridden for compatibility reasons. See https://webcompat.com/issues/1804 for details."
);

const CHROME_UA = navigator.userAgent + " Chrome for WebCompat";

const nav = Object.getPrototypeOf(navigator.wrappedJSObject);

const ua = Object.getOwnPropertyDescriptor(nav, "userAgent");
ua.get = exportFunction(() => CHROME_UA, window);
Object.defineProperty(nav, "userAgent", ua);

const vendor = Object.getOwnPropertyDescriptor(nav, "vendor");
vendor.get = exportFunction(() => "Google Inc.", window);
Object.defineProperty(nav, "vendor", vendor);
