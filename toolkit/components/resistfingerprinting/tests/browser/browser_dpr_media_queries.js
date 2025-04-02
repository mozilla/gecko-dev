/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/**
 * Bug 1954493 - Make sure devicePixelRatio and media queries are coherent.
 *
 * We make sure that passing devicePixelRatio (as dppx) to min-resolution and
 * max-resolution matches.
 *
 * We also make sure that the devicePixelRatios matches the non-rfp values.
 */

const DPRs = {};
let tab;

add_setup(async () => {
  // We spoof the devicePixelRatio to 2 in RFP.
  await SpecialPowers.pushPrefEnv({
    set: [["layout.css.devPixelsPerPx", 2]],
  });

  const emptyPage =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.com"
    ) + "empty.html";
  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, emptyPage);

  for (let auPerPx = 120; auPerPx > 0; auPerPx--) {
    const zoom = 60 / auPerPx;
    DPRs[zoom] = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [zoom],
      zoomLevel => {
        const { Layout } = ChromeUtils.importESModule(
          "chrome://mochitests/content/browser/accessible/tests/browser/Layout.sys.mjs"
        );
        Layout.zoomDocument(content.document, zoomLevel);
        // Workaround: content.devicePixelRatio has the unspoofed value.
        return content.wrappedJSObject.devicePixelRatio;
      }
    );
  }
  await SpecialPowers.popPrefEnv();
});

async function runForZoomLevel(zoom) {
  const [dpr, matchMin, matchMax] = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [zoom],
    zoomLevel => {
      const { Layout } = ChromeUtils.importESModule(
        "chrome://mochitests/content/browser/accessible/tests/browser/Layout.sys.mjs"
      );
      Layout.zoomDocument(content.document, zoomLevel);
      // Workaround: content.devicePixelRatio has the unspoofed value.
      const dpr = content.wrappedJSObject.devicePixelRatio;
      return [
        dpr,
        content.matchMedia(`(min-resolution: ${dpr}dppx)`).matches,
        content.matchMedia(`(max-resolution: ${dpr}dppx)`).matches,
      ];
    }
  );
  is(DPRs[zoom], dpr, `devicePixelRatio is ${dpr} for zoom ${zoom}`);
  is(matchMin, true, `min-resolution matches DPR for zoom ${zoom}`);
  is(matchMax, true, `max-resolution matches DPR for zoom ${zoom}`);
}

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.resistFingerprinting", true]],
  });

  for (let auPerPx = 120; auPerPx > 0; auPerPx--) {
    await runForZoomLevel(60 / auPerPx);
  }

  // Run the test again with a different textScaleFactor.
  // This is to make sure that the devicePixelRatio is not affected by
  // the textScaleFactor.
  await SpecialPowers.pushPrefEnv({
    set: [["ui.textScaleFactor", 200]],
  });

  for (let auPerPx = 120; auPerPx > 0; auPerPx--) {
    await runForZoomLevel(60 / auPerPx);
  }

  BrowserTestUtils.removeTab(tab);
});
