/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

const { MODE_DISABLED, MODE_REJECT, MODE_REJECT_OR_ACCEPT, MODE_UNSET } =
  Ci.nsICookieBannerService;

const TEST_MODES = [
  MODE_DISABLED,
  MODE_REJECT,
  MODE_REJECT_OR_ACCEPT,
  MODE_UNSET, // Should be recorded as invalid.
  99, // Invalid
  -1, // Invalid
];

function convertModeToTelemetryString(mode) {
  switch (mode) {
    case MODE_DISABLED:
      return "disabled";
    case MODE_REJECT:
      return "reject";
    case MODE_REJECT_OR_ACCEPT:
      return "reject_or_accept";
  }

  return "invalid";
}

/**
 * A helper function to verify the reload telemetry.
 *
 * @param {Number} length The expected length of the telemetry array.
 * @param {Number} idx The index of the telemetry to be verified.
 * @param {Object} expected An object that describe the expected value.
 */
function verifyReloadTelemetry(length, idx, expected) {
  let events = Glean.cookieBanners.reload.testGetValue();

  is(events.length, length, "There is a expected number of reload events.");

  let event = events[idx];

  let { noRule, hasCookieRule, hasClickRule } = expected;
  is(event.name, "reload", "The reload event has the correct name");
  is(event.extra.no_rule, noRule, "The extra field 'no_rule' is expected");
  is(
    event.extra.has_cookie_rule,
    hasCookieRule,
    "The extra field 'has_cookie_rule' is expected"
  );
  is(
    event.extra.has_click_rule,
    hasClickRule,
    "The extra field 'has_click_rule' is expected"
  );
}

/**
 * A helper function to reload the browser and wait until it loads.
 *
 * @param {Browser} browser The browser object.
 * @param {String} url The URL to be loaded.
 */
async function reloadBrowser(browser, url) {
  let reloaded = BrowserTestUtils.browserLoaded(browser, false, url);

  // Reload as a user.
  window.BrowserReload();

  await reloaded;
}
/**
 * A helper function to open the testing page for look up telemetry.
 *
 * @param {browser} browser The browser element
 * @param {boolean} testInTop To indicate the page should be opened in top level
 * @param {String} page The url of the testing page
 * @param {String} domain The domain of the testing page
 */
async function openLookUpTelemetryTestPage(browser, testInTop, page, domain) {
  let clickFinishPromise = promiseBannerClickingFinish(domain);

  if (testInTop) {
    BrowserTestUtils.loadURI(browser, page);
  } else {
    BrowserTestUtils.loadURI(browser, TEST_ORIGIN_C);
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(browser, [page], async testURL => {
      let iframe = content.document.createElement("iframe");
      iframe.src = testURL;
      content.document.body.appendChild(iframe);
      await ContentTaskUtils.waitForEvent(iframe, "load");
    });
  }

  await clickFinishPromise;
}

add_setup(async function () {
  // Clear telemetry before starting telemetry test.
  Services.fog.testResetFOG();

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("cookiebanners.service.mode");
    Services.prefs.clearUserPref("cookiebanners.service.mode.privateBrowsing");
    if (
      Services.prefs.getIntPref("cookiebanners.service.mode") !=
        Ci.nsICookieBannerService.MODE_DISABLED ||
      Services.prefs.getIntPref("cookiebanners.service.mode.privateBrowsing") !=
        Ci.nsICookieBannerService.MODE_DISABLED
    ) {
      // Restore original rules.
      Services.cookieBanners.resetRules(true);
    }

    // Clear cookies that have been set during testing.
    await SiteDataTestUtils.clear();
  });
});

add_task(async function test_service_mode_telemetry() {
  let service = Cc["@mozilla.org/cookie-banner-service;1"].getService(
    Ci.nsIObserver
  );

  for (let mode of TEST_MODES) {
    for (let modePBM of TEST_MODES) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["cookiebanners.service.mode", mode],
          ["cookiebanners.service.mode.privateBrowsing", modePBM],
        ],
      });

      // Trigger the idle-daily on the cookie banner service.
      service.observe(null, "idle-daily", null);

      // Verify the telemetry value.
      for (let label of ["disabled", "reject", "reject_or_accept", "invalid"]) {
        let expected = convertModeToTelemetryString(mode) == label;
        let expectedPBM = convertModeToTelemetryString(modePBM) == label;

        is(
          Glean.cookieBanners.normalWindowServiceMode[label].testGetValue(),
          expected,
          `Has set label ${label} to ${expected} for mode ${mode}.`
        );
        is(
          Glean.cookieBanners.privateWindowServiceMode[label].testGetValue(),
          expectedPBM,
          `Has set label '${label}' to ${expected} for mode ${modePBM}.`
        );
      }

      await SpecialPowers.popPrefEnv();
    }
  }
});

add_task(async function test_service_detectOnly_telemetry() {
  let service = Cc["@mozilla.org/cookie-banner-service;1"].getService(
    Ci.nsIObserver
  );

  for (let detectOnly of [true, false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["cookiebanners.service.detectOnly", detectOnly]],
    });

    // Trigger the idle-daily on the cookie banner service.
    service.observe(null, "idle-daily", null);

    is(
      Glean.cookieBanners.serviceDetectOnly.testGetValue(),
      detectOnly,
      `Has set detect-only metric to ${detectOnly}.`
    );

    await SpecialPowers.popPrefEnv();
  }
});
