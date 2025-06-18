/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bugs 1287715 - Audio intermittently does not work.
 *
 * The site relies on unspecified behavior in Chromium and WebKit where
 * they will defer firing loadedmetadata events until after a listener
 * for the event is attached. We can mimic that behavior here.
 */

/* globals exportFunction */

console.info(
  "loadedmetadata event is being deferred until GAME_READY. See https://bugzilla.mozilla.org/show_bug.cgi?id=1287715 for details."
);

let loadedmetadataEvent;
let loadedmetadataListener;

const { prototype } = window.wrappedJSObject.EventTarget;
const { addEventListener, dispatchEvent } = prototype;

Object.defineProperty(prototype, "addEventListener", {
  value: exportFunction(function (type, listener, cfg) {
    if (type?.toLowerCase() === "loadedmetadata") {
      loadedmetadataListener = listener;
      return addEventListener.call(
        this,
        type,
        exportFunction(e => {
          loadedmetadataEvent = e;
        }, window),
        cfg
      );
    }
    return addEventListener.call(this, type, listener, cfg);
  }, window),
});

Object.defineProperty(prototype, "dispatchEvent", {
  value: exportFunction(function (e) {
    if (
      e?.type === "GAME_READY" &&
      loadedmetadataEvent &&
      loadedmetadataListener
    ) {
      try {
        (loadedmetadataListener?.handleEvent ?? loadedmetadataListener)(
          loadedmetadataEvent
        );
      } catch (_) {
        console.trace(_);
      }
      loadedmetadataListener = undefined;
      loadedmetadataEvent = undefined;
    }
    return dispatchEvent.call(this, e);
  }, window),
});
