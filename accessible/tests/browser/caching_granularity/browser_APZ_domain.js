/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

// CacheKey::VisualViewportOffset, CacheDomain::APZ
addAccessibleTask(
  `<br><div id="test">test</div>`,
  async function (browser, docAcc, contentDoc) {
    // Request this on the top level doc, it won't be cached on any other doc
    await testAttributeCachePresence(
      contentDoc ? contentDoc : docAcc,
      "voffset",
      async () => {
        // Call this to activate the APZ domain, if we don't do this
        // first we will drop the queued cache update triggered by
        // the subsequent pinchZoomInWithTouch
        docAcc.getBounds({}, {}, {}, {});
        // Doing an APZ queries the visual viewport offset info.
        info("Pinch zooming...");
        await SpecialPowers.spawn(browser, [], async () => {
          const visualScrollPromise = new Promise(resolve => {
            content.window.visualViewport.addEventListener("scroll", resolve, {
              once: true,
            });
          });
          const utils = SpecialPowers.getDOMWindowUtils(content.window);
          utils.setResolutionAndScaleTo(2);
          utils.scrollToVisual(
            200,
            200,
            utils.UPDATE_TYPE_MAIN_THREAD,
            utils.SCROLL_MODE_INSTANT
          );
          await visualScrollPromise;
        });
      }
    );
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);
