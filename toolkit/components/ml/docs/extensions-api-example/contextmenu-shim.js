/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* eslint-env webextensions */
"use strict";

// Attach a listener to each image in the page.
document.querySelectorAll("img").forEach(node => {
  // The `contextmenu` event is used to detect a long-press on Android.
  node.addEventListener("contextmenu", event => {
    if (event.target?.src) {
      event.preventDefault();

      browser.runtime.sendMessage({
        type: "generate-alt-text",
        data: { url: event.target.src },
      });
    }
  });
});
