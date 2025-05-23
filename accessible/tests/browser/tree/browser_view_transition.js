/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

/**
 * Verify that the view transition shouldn't get an accessible.
 */
addAccessibleTask(
  `
<style>
  ::view-transition-group(*),
  ::view-transition-image-pair(*),
  ::view-transition-old(*),
  ::view-transition-new(*) {
    animation-play-state: paused;
  }
  ::view-transition {
    background-color: blue;
  }
</style>
<div style="view-transition-name: target">target</div>
  `,
  async function (browser, docAcc) {
    const originalTree = { DOCUMENT: [{ SECTION: [{ TEXT_LEAF: [] }] }] };

    // Initial check.
    testAccessibleTree(docAcc, originalTree);

    info("Starting view transition");
    await invokeContentTask(browser, [], async () => {
      let vt = content.document.startViewTransition();
      await vt.ready;
    });

    info("Checking the existence of the view transition");
    await invokeContentTask(browser, [], async () => {
      is(
        content.getComputedStyle(
          content.document.documentElement,
          "::view-transition"
        ).backgroundColor,
        "rgb(0, 0, 255)",
        "The active view transition is animating"
      );
    });

    // The accessibility tree should not be changed with an active view
    // transition.
    testAccessibleTree(docAcc, originalTree);
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    chrome: true,
  }
);
