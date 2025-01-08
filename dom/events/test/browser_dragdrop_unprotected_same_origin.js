/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that drag and drop events are sent at the right time. Includes tests for
// dragging between windows and iframes, all of the same origin.
// dom.events.dataTransfer.protected.enabled = false

"use strict";

const kBaseUrl = "https://example.org/browser/dom/events/test/";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.events.dataTransfer.protected.enabled", false]],
  });
  registerCleanupFunction(async function () {
    SpecialPowers.popPrefEnv();
  });

  await setup({
    outerURL1: kBaseUrl + "browser_dragdrop_outer.html",
    outerURL2: kBaseUrl + "browser_dragdrop_outer.html",
    innerURL1: kBaseUrl + "browser_dragdrop_inner.html",
    innerURL2: kBaseUrl + "browser_dragdrop_inner.html",
  });
});

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/events/test/browser_dragdrop_impl.js",
  this
);
