/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Drag and drop stuff from inside one shadow DOM to another shadow DOM.
// Includes shadow DOMs in iframes.

"use strict";

const kBaseUrl1 = "https://example.org/browser/dom/events/test/";
const kBaseUrl2 = "https://example.com/browser/dom/events/test/";

add_setup(async function () {
  await setup({
    outerURL1: kBaseUrl1 + "browser_dragdrop_shadow_outer.html",
    outerURL2: kBaseUrl2 + "browser_dragdrop_shadow_outer.html",
    innerURL1: kBaseUrl1 + "browser_dragdrop_shadow_inner.html",
    innerURL2: kBaseUrl2 + "browser_dragdrop_shadow_inner.html",
  });
});

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/events/test/browser_dragdrop_impl.js",
  this
);
