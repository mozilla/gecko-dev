/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const emptyPage =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty.html";

const spoofedLocale = "en-US";
const alternativeLocale = "tr-TR";

const getDateString = async () => {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    emptyPage,
  });

  const locale = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [alternativeLocale],
    function (locale) {
      SpecialPowers.Cu.getJSTestingFunctions().setDefaultLocale(locale);

      return content.eval(`Intl.DateTimeFormat().resolvedOptions().locale`);
    }
  );

  await BrowserTestUtils.removeTab(tab);

  return locale;
};

const testWithPrefs = async prefs => {
  const locale = await getDateString();
  is(locale, alternativeLocale, "Locale is set to tr-TR");

  await SpecialPowers.pushPrefEnv({
    set: prefs,
  });

  const localeRFP = await getDateString();
  is(localeRFP, spoofedLocale, "Locale is set to en-US");

  await SpecialPowers.popPrefEnv();
};

add_task(async function test_rfp() {
  await testWithPrefs([
    ["privacy.resistFingerprinting", true],
    ["privacy.spoof_english", 2],
  ]);
});

add_task(async function test_fpp() {
  await testWithPrefs([
    ["privacy.fingerprintingProtection", true],
    ["privacy.fingerprintingProtection.overrides", "-AllTargets,+JSLocale"],
    ["privacy.spoof_english", 2],
  ]);
});
