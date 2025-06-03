/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["test.wait300msAfterTabSwitch", true]],
  });

  await UrlClassifierTestUtils.addTestTrackers();
  // Extend clickjacking delay for test because timer expiry can happen before we
  // check the toggle is disabled (especially in chaos mode).
  Services.prefs.setIntPref(SEC_DELAY_PREF, 1000);
  Services.prefs.setBoolPref(TRACKING_PREF, true);
  Services.prefs.setBoolPref(SMARTBLOCK_EMBEDS_ENABLED_PREF, true);

  registerCleanupFunction(() => {
    UrlClassifierTestUtils.cleanupTestTrackers();
    Services.prefs.clearUserPref(TRACKING_PREF);
  });

  Services.fog.testResetFOG();
});

add_task(async function test_smartblock_embed_replaced_after_init_load() {
  // Open a site with a test "embed"
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    waitForLoad: true,
  });

  await loadSmartblockPageOnTab(tab);

  // Check TP enabled
  const TrackingProtection = gProtectionsHandler.blockers.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the tab");
  ok(TrackingProtection.enabled, "TP is enabled");

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // Check that the "embed" was replaced with a placeholder
    let placeholders = content.document.querySelectorAll(
      ".shimmed-embedded-content"
    );
    is(placeholders.length, 1, "Embed is replaced with a placeholder");
  });

  let smartblockScriptFinished = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "smartblockEmbedScriptFinished",
    false,
    null,
    true
  );

  // inject a second placeholder into the page
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    let newPlaceholder = content.document.createElement("div");
    newPlaceholder.classList.add("broken-embed-content");

    content.document.body.appendChild(newPlaceholder);
  });

  // wait for replacement to run
  await smartblockScriptFinished;

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // Check that the second "embed" was replaced with a placeholder
    let placeholders = content.document.querySelectorAll(
      ".shimmed-embedded-content"
    );
    is(placeholders.length, 2, "Embed is replaced with a placeholder");
  });

  await BrowserTestUtils.removeTab(tab);
});
