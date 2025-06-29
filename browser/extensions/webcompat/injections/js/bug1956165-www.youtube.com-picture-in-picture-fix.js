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
  "exitFullscreen and window.outerWidth|Height have been overridden for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1956165 for details."
);

const win = window.wrappedJSObject;
const outerWidthDesc = Object.getOwnPropertyDescriptor(win, "outerWidth");
const outerHeightDesc = Object.getOwnPropertyDescriptor(win, "outerHeight");
const originalOuterWidth = outerWidthDesc.get;
const originalOuterHeight = outerHeightDesc.get;

// This is the logic YouTube uses to detect the app backgrounding to enter picture in picture mode mode (as of May 28 2025).

const getRatio = (() => {
  let cachedRatio;
  return function () {
    if (cachedRatio === undefined) {
      const cfg = window.wrappedJSObject.ytcfg.get(
        "WEB_PLAYER_CONTEXT_CONFIGS"
      );
      const experiment =
        cfg?.WEB_PLAYER_CONTEXT_CONFIG_ID_MWEB_WATCH?.serializedExperimentFlags?.match(
          /html5_picture_in_picture_logging_onresize_ratio=(\d+(\.\d+)?)/
        )?.[1];
      cachedRatio = parseFloat(experiment) || 0.33;
    }
    return cachedRatio;
  };
})();

const inPipMode = (() => {
  let o_ = 0;
  return function () {
    const l = window.screen.width * window.screen.height;
    const M = originalOuterWidth() * originalOuterHeight();
    o_ = Math.max(o_, l, M);
    return M / o_ < getRatio();
  };
})();

outerWidthDesc.get = exportFunction(function () {
  if (inPipMode()) {
    return screen.width;
  }
  return originalOuterWidth();
}, window);
outerHeightDesc.get = exportFunction(() => {
  if (inPipMode()) {
    return screen.height;
  }
  return originalOuterHeight();
}, window);
Object.defineProperty(win, "outerWidth", outerWidthDesc);
Object.defineProperty(win, "outerHeight", outerHeightDesc);

const originalExitFullscreen = win.Document.prototype.exitFullscreen;

const newExitFullscreen = exportFunction(function () {
  if (inPipMode()) {
    return undefined;
  }
  return originalExitFullscreen.apply(this);
}, window);

Object.defineProperty(win.Document.prototype, "exitFullscreen", {
  value: newExitFullscreen,
  writable: true,
  configurable: true,
});
