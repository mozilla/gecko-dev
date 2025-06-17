/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

let { UrlClassifierTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlClassifierTestUtils.sys.mjs"
);

const ANTIFRAUD_DOMAIN = "https://anti-fraud.example.org/";
const ANTIFRAUD_IMAGE = ANTIFRAUD_DOMAIN + TEST_PATH + "raptor.jpg";

async function tryLoadingImageAndCheckAntiFraudFlags(tab, shouldLoad) {
  let channelCheckPromise = checkChannelClassificationsFlags(
    ANTIFRAUD_DOMAIN,
    Ci.nsIClassifiedChannel.CLASSIFIED_ANTIFRAUD
  );

  let isImageLoaded = await loadImage(tab.linkedBrowser, ANTIFRAUD_IMAGE);

  if (shouldLoad) {
    ok(isImageLoaded, "Image should be loaded");
  } else {
    ok(!isImageLoaded, "Image should not be loaded");
  }

  return channelCheckPromise;
}

add_setup(async function () {
  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(function () {
    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "urlclassifier.features.antifraud.annotate.blocklistTables",
        "mochitest7-track-simple",
      ],

      ["privacy.trackingprotection.enabled", true],
      ["privacy.trackingprotection.fingerprinting.enabled", true],
      ["privacy.trackingprotection.annotate_channels", true],
      ["privacy.trackingprotection.cryptomining.enabled", false],
      ["privacy.trackingprotection.emailtracking.enabled", false],
      ["privacy.trackingprotection.socialtracking.enabled", false],
      ["privacy.trackingprotection.consentmanager.annotate_channels", false],
      ["privacy.trackingprotection.antifraud.annotate_channels", true],

      ["privacy.trackingprotection.antifraud.skip.enabled", false],
      ["privacy.trackingprotection.antifraud.skip.pbmode.enabled", false],
    ],
  });
});

add_task(async function test_antifraud_annotation_unblocking_off() {
  // check antifraud still blocked when skip is disabled
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  await tryLoadingImageAndCheckAntiFraudFlags(tab, false);

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_antifraud_annotation_unblocking_on() {
  // check that antifraud not blocked when skip is enabled
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.trackingprotection.antifraud.skip.enabled", true],
      ["privacy.trackingprotection.antifraud.skip.pbmode.enabled", true],
    ],
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  await tryLoadingImageAndCheckAntiFraudFlags(tab, true);

  await BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
