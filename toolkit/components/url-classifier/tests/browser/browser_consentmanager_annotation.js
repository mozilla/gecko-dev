/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

let { UrlClassifierTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlClassifierTestUtils.sys.mjs"
);

const CONSENTMANAGER_DOMAIN = "https://consent-manager.example.org/";
const CONSENTMANAGER_IMAGE = CONSENTMANAGER_DOMAIN + TEST_PATH + "raptor.jpg";

async function tryLoadingImageAndCheckConsentManagerFlags(tab, shouldLoad) {
  let channelCheckPromise = checkChannelClassificationsFlags(
    CONSENTMANAGER_DOMAIN,
    Ci.nsIClassifiedChannel.CLASSIFIED_CONSENTMANAGER
  );

  let isImageLoaded = await loadImage(tab.linkedBrowser, CONSENTMANAGER_IMAGE);

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
        "urlclassifier.features.consentmanager.annotate.blocklistTables",
        "mochitest6-track-simple",
      ],

      ["privacy.trackingprotection.enabled", true],
      ["privacy.trackingprotection.annotate_channels", true],
      ["privacy.trackingprotection.cryptomining.enabled", false],
      ["privacy.trackingprotection.emailtracking.enabled", false],
      ["privacy.trackingprotection.fingerprinting.enabled", false],
      ["privacy.trackingprotection.socialtracking.enabled", false],
      ["privacy.trackingprotection.antifraud.annotate_channels", false],
      ["privacy.trackingprotection.consentmanager.annotate_channels", true],

      ["privacy.trackingprotection.consentmanager.skip.enabled", false],
      ["privacy.trackingprotection.consentmanager.skip.pbmode.enabled", false],
    ],
  });
});

add_task(async function test_consentmanager_annotation_unblocking_off() {
  // check consent manager still blocked when skip is disabled
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  await tryLoadingImageAndCheckConsentManagerFlags(tab, false);

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_consentmanager_annotation_unblocking_on() {
  // check that consent manager not blocked when skip is enabled
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.trackingprotection.consentmanager.skip.enabled", true],
      ["privacy.trackingprotection.consentmanager.skip.pbmode.enabled", true],
    ],
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  await tryLoadingImageAndCheckConsentManagerFlags(tab, true);

  await BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_consentmanager_annotation_unblocking_pbmode() {
  // check that consent manager still blocks in regular windows and doesn't block in
  // private windows
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.trackingprotection.consentmanager.skip.pbmode.enabled", true],
    ],
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);

  await tryLoadingImageAndCheckConsentManagerFlags(tab, false);

  await BrowserTestUtils.removeTab(tab);

  // Next do same thing in private window
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let privateTab = await BrowserTestUtils.openNewForegroundTab(
    privateWindow.gBrowser,
    TEST_PAGE
  );

  await tryLoadingImageAndCheckConsentManagerFlags(privateTab, true);

  await BrowserTestUtils.removeTab(privateTab);
  await BrowserTestUtils.closeWindow(privateWindow);
  await SpecialPowers.popPrefEnv();
});
