/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1850998 - going "fullscreen" does not hide the address bar
 *
 * The site avoids calling requestFullScreen on Firefox. We can address
 * this by pretending to not be Firefox for that specific check.
 */

/* globals exportFunction */

const proto = window.wrappedJSObject.RegExp.prototype;
const descriptor = Object.getOwnPropertyDescriptor(proto, "test");
const { value } = descriptor;

descriptor.value = exportFunction(function (test) {
  if (this.source === "UCBrowser|Firefox|SamsungBrowser") {
    return false;
  }
  return value.call(this, test);
}, window);

Object.defineProperty(proto, "test", descriptor);
