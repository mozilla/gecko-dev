/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1956165 - Fix picture-in-picture mode on Mobile YouTube
 *
 * YouTube does not play well with our picture in picture implementation, and
 * effectively cancels it. We can work around this conflict with this site patch.
 */

/* globals exportFunction */

console.info(
  "exitFullscreen and pause have been overridden for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1956165 for details."
);

const originalPause = window.wrappedJSObject.HTMLMediaElement.prototype.pause;

const newPause = exportFunction(function (...args) {
  if (this.ownerDocument.inAndroidPipMode) {
    return undefined;
  }
  return originalPause.apply(this, args);
}, window);

Object.defineProperty(
  window.wrappedJSObject.HTMLMediaElement.prototype,
  "pause",
  {
    value: newPause,
    writable: true,
    configurable: true,
  }
);

const originalExitFullscreen =
  window.wrappedJSObject.Document.prototype.exitFullscreen;

const newExitFullscreen = exportFunction(function (...args) {
  if (!this.ownerDocument.inAndroidPipMode) {
    return undefined;
  }
  return originalExitFullscreen.apply(this, args);
}, window);

Object.defineProperty(
  window.wrappedJSObject.Document.prototype,
  "exitFullscreen",
  {
    value: newExitFullscreen,
    writable: true,
    configurable: true,
  }
);
