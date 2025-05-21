/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals exportFunction */

/**
 * Bug 1911423 - app.powerbi.com - zooming is broken on maps
 *
 * They listen for non-standard mousewheel events, rather than wheel,
 * which breaks zooming. This emulates mousewheel events for them.
 */

console.info(
  "Emulating mousewheel events for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1911423 for details."
);

(function () {
  const { prototype } = window.wrappedJSObject.WheelEvent;
  Object.defineProperty(prototype, "type", {
    configurable: true,
    get: exportFunction(() => "mousewheel", window),
    set: exportFunction(() => {}, window),
  });
})();

(function () {
  const { prototype } = window.wrappedJSObject.EventTarget;
  const { addEventListener } = prototype;
  prototype.addEventListener = exportFunction(function (type, fn, c, d) {
    if (type === "mousewheel") {
      type = "wheel";
    }
    return addEventListener.call(this, type, fn, c, d);
  }, window);
})();
