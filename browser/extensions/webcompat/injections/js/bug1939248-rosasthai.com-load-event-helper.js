/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1939248 - Load event listener issue on rosasthai.com
 * WebCompat issue #145642 - https://webcompat.com/issues/145642
 *
 * The site assumes that its code will run before the window load event, and
 * so their buttons will not function properly if that is untrue. We can call
 * those event listeners immediately as they try to add them, if the document
 * readyState is already "complete", which fixes this bug.
 */

/* globals exportFunction */

console.info(
  "running late window load listeners for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1939248 for details."
);

(function runLateWindowLoadEventListenersImmediately() {
  const { prototype } = window.wrappedJSObject.EventTarget;
  const { addEventListener } = prototype;
  prototype.addEventListener = exportFunction(function (type, b, c, d) {
    if (
      this !== window ||
      document?.readyState !== "complete" ||
      type?.toLowerCase() !== "load"
    ) {
      return addEventListener.call(this, type, b, c, d);
    }
    console.log("window.addEventListener(load) called too late, so calling", b);
    try {
      b?.call();
    } catch (e) {
      console.error(e);
    }
    return undefined;
  }, window);
})();
