/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const emptyPage = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

/**
 * Bug 1889762 - Testing the timezone offset override in the service worker
 * to ensure that the override is applied correctly.
 * Same as browser_fpiServiceWorkers_fingerprinting.js but uses serviceWorkerTester.js
 * instead of serviceWorker.js
 */

let expected = null;

const runTest = async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", true],
      [
        "privacy.fingerprintingProtection.overrides",
        "-AllTargets,+CanvasRandomization",
      ],
      ["privacy.resistFingerprinting", false],
    ],
  });

  const noFPITab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    emptyPage + "nofpi"
  );

  const func = async () => {
    const canvas = new OffscreenCanvas(3, 1);
    const ctx = canvas.getContext("2d");

    ctx.fillStyle = "rgb(255, 0, 0)";
    ctx.fillRect(0, 0, 1, 1);
    ctx.fillStyle = "rgb(0, 255, 0)";
    ctx.fillRect(1, 0, 1, 1);
    ctx.fillStyle = "rgb(0, 0, 255)";
    ctx.fillRect(2, 0, 1, 1);

    return ctx.getImageData(0, 0, 3, 1).data.join(",");
  };

  const extractionWithoutFPI = await runFunctionInServiceWorker(
    noFPITab.linkedBrowser,
    func
  );

  await SpecialPowers.pushPrefEnv({
    set: [["privacy.firstparty.isolate", true]],
  });

  const withFPITab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    emptyPage + "withFPI"
  );

  const extractionWithFPI = await runFunctionInServiceWorker(
    withFPITab.linkedBrowser,
    func
  );

  await SpecialPowers.popPrefEnv();

  isnot(
    extractionWithoutFPI,
    extractionWithFPI,
    `Canvas data should be different with FPI enabled because origin attributes are different`
  );

  BrowserTestUtils.removeTab(noFPITab);
  BrowserTestUtils.removeTab(withFPITab);
  await SpecialPowers.popPrefEnv();
};

add_task(async function check_with_fpi_enabled() {
  await runTest();
});
