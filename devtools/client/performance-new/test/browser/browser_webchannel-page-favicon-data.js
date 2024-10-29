/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const FRONTEND_URL =
  "https://example.com/browser/devtools/client/performance-new/test/browser/webchannel-favicons.html";
const TEST_FAVICON =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAQMAAAAlPW0iAAAAAXNSR0IB2cksfwAAAAlwSFlzAAALEwAACxMBAJqcGAAAAANQTFRFAP//GVwvJQAAAA5JREFUeJxjZGBgJAUBAAHIABFDZFrTAAAAAElFTkSuQmCC";

const PAGE_URL = "https://profiler.firefox.com";
const FAVICON_URL = "https://profiler.firefox.com/favicon.ico";

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

add_setup(() => {
  registerCleanupFunction(async () => {
    PlacesUtils.favicons.expireAllFavicons();
    await PlacesUtils.history.clear();
  });
});

add_task(async function test() {
  info("Test the WebChannel mechanism works for getting the page favicon data");

  await PlacesTestUtils.addVisits(PAGE_URL);
  await PlacesUtils.favicons.setFaviconForPage(
    Services.io.newURI(PAGE_URL),
    Services.io.newURI(FAVICON_URL),
    Services.io.newURI(TEST_FAVICON)
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: FRONTEND_URL,
    },
    async () => {
      await waitForTabTitle("Favicons received");
      ok(true, "The favicons are successfully retrieved by the WebChannel.");
    }
  );
});
