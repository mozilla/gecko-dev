/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1960595 - 'browser not fully unsupported' banner is shown on Android
 *
 * The site reads navigator.userAgent with a specific regexp to show the
 * banner, and since it has no obvious CSS to target, and the same banner
 * mechanism may be used for other purposes, we surgically return false
 * for that regexp's test to prevent the banner from showing.
 */

/* globals exportFunction */

const proto = window.wrappedJSObject.RegExp.prototype;
const descriptor = Object.getOwnPropertyDescriptor(proto, "test");
const { value } = descriptor;

descriptor.value = exportFunction(function (test) {
  if (this.source === "^(?!.*Seamonkey)(?=.*Firefox).*") {
    descriptor.value = value;
    Object.defineProperty(proto, "test", descriptor);
    return false;
  }
  return value.call(this, test);
}, window);

Object.defineProperty(proto, "test", descriptor);
