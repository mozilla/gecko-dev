/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* global getJSTestingFunctions */

const emptyPage =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty.html";

/**
 * Bug 1889762 - Testing the timezone offset override in the workers to ensure
 * that the override is applied correctly.
 */

const runTest = async enabled => {
  const overrides = enabled ? "+JSDateTimeUTC" : "-JSDateTimeUTC";
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", true],
      ["privacy.fingerprintingProtection.overrides", overrides],
      ["privacy.resistFingerprinting", false],
    ],
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, emptyPage);

  const timeZoneOffset = await runFunctionInWorker(
    tab.linkedBrowser,
    async () => {
      await getJSTestingFunctions().setTimeZone("PST8PDT");
      const date = new Date();
      const offset = date.getTimezoneOffset();
      await getJSTestingFunctions().setTimeZone(undefined);
      return offset;
    }
  );

  info("Actual: " + new Date().getTimezoneOffset() + " Got: " + timeZoneOffset);
  const expected = enabled ? 0 : 420;
  is(
    timeZoneOffset,
    expected,
    `Timezone offset should be ${expected} in the service worker`
  );

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
};

add_task(async function check_overrides_enabled() {
  await runTest(true);
});

add_task(async function check_overrides_disabled() {
  await runTest(false);
});
