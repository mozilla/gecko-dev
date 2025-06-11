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

add_task(async function test_smartblock_embed_replaced() {
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

  await clickOnPagePlaceholder(tab);

  // Check telemetry is triggered
  let protectionsPanelOpenEvents =
    Glean.securityUiProtectionspopup.openProtectionsPopup.testGetValue();
  is(
    protectionsPanelOpenEvents.length,
    1,
    "Protections panel open telemetry has correct embeds shown value"
  );
  is(
    protectionsPanelOpenEvents[0].extra.openingReason,
    "embedPlaceholderButton",
    "Protections panel open telemetry has correct opening reason"
  );
  is(
    protectionsPanelOpenEvents[0].extra.smartblockEmbedTogglesShown,
    "true",
    "Protections panel open telemetry has correct toggles shown value"
  );

  // Check smartblock section is unhidden
  ok(
    BrowserTestUtils.isVisible(
      gProtectionsHandler._protectionsPopupSmartblockContainer
    ),
    "Smartblock section is visible"
  );

  // Check toggle is present and off
  let blockedEmbedToggle =
    gProtectionsHandler._protectionsPopupSmartblockToggleContainer
      .firstElementChild;
  ok(blockedEmbedToggle, "Toggle exists in container");
  ok(BrowserTestUtils.isVisible(blockedEmbedToggle), "Toggle is visible");
  ok(!blockedEmbedToggle.pressed, "Unblock toggle should be off");

  // Check toggle disabled by clickjacking protections
  ok(blockedEmbedToggle.disabled, "Unblock toggle should be disabled");

  // Wait for clickjacking protections to timeout
  let delayTime = Services.prefs.getIntPref(SEC_DELAY_PREF);
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, delayTime + 100));

  // Setup promise on custom event to wait for placeholders to finish replacing
  let embedScriptFinished = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "testEmbedScriptFinished",
    false,
    null,
    true
  );

  // Click to toggle to unblock embed and wait for script to finish
  await EventUtils.synthesizeMouseAtCenter(blockedEmbedToggle.buttonEl, {});

  await embedScriptFinished;

  ok(blockedEmbedToggle.pressed, "Unblock toggle should be on");

  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    let unloadedEmbed = content.document.querySelector(".broken-embed-content");
    ok(!unloadedEmbed, "Unloaded embeds should not be on the page");

    // Check embed was put back on the page
    let loadedEmbed = content.document.querySelector(".loaded-embed-content");
    ok(loadedEmbed, "Embed should now be on the page");
  });

  // Check toggle telemetry is triggered
  let toggleEvents =
    Glean.securityUiProtectionspopup.clickSmartblockembedsToggle.testGetValue();
  is(toggleEvents.length, 1, "Telemetry triggered for toggle press");
  is(
    toggleEvents[0].extra.isBlock,
    "false",
    "Toggle press telemetry is an unblock"
  );
  is(
    toggleEvents[0].extra.openingReason,
    "embedPlaceholderButton",
    "Smartblock shown event has correct reason"
  );

  // close and open protections panel
  await closeProtectionsPanel(window);

  // Verify telemetry after close
  let protectionsPanelClosedEvents =
    Glean.securityUiProtectionspopup.closeProtectionsPopup.testGetValue();
  is(
    protectionsPanelClosedEvents.length,
    1,
    "Telemetry triggered for protections panel closed"
  );
  is(
    protectionsPanelClosedEvents[0].extra.smartblockToggleClicked,
    "true",
    "Protections panel closed telemetry shows toggle was clicked"
  );
  is(
    protectionsPanelClosedEvents[0].extra.openingReason,
    "embedPlaceholderButton",
    "Protections panel closed event has correct reason"
  );

  await openProtectionsPanel(window);

  // Check if smartblock section is still there after unblock
  ok(
    BrowserTestUtils.isVisible(
      gProtectionsHandler._protectionsPopupSmartblockContainer
    ),
    "Smartblock section is visible"
  );

  // Check toggle is still there and is on now
  blockedEmbedToggle =
    gProtectionsHandler._protectionsPopupSmartblockToggleContainer
      .firstElementChild;
  ok(blockedEmbedToggle, "Toggle exists in container");
  ok(BrowserTestUtils.isVisible(blockedEmbedToggle), "Toggle is visible");
  ok(blockedEmbedToggle.pressed, "Unblock toggle should be on");

  // Check protections panel open telemetry
  // Check telemetry is triggered
  protectionsPanelOpenEvents =
    Glean.securityUiProtectionspopup.openProtectionsPopup.testGetValue();
  is(
    protectionsPanelOpenEvents.length,
    2,
    "Protections panel open telemetry has correct embeds shown value"
  );
  // Note: the openingReason shows as undefined since the test opened the
  // protections panel directly with a function call"
  is(
    protectionsPanelOpenEvents[1].extra.openingReason,
    undefined,
    "Protections panel open telemetry has correct opening reason"
  );
  is(
    protectionsPanelOpenEvents[1].extra.smartblockEmbedTogglesShown,
    "true",
    "Protections panel open telemetry has correct toggles shown value"
  );

  // Setup promise on custom event to wait for placeholders to finish replacing
  let smartblockScriptFinished = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "smartblockEmbedScriptFinished",
    false,
    null,
    true
  );

  // click toggle to reblock (this will trigger a reload)
  // Note: clickjacking delay should not happen because panel not opened via embed button
  await EventUtils.synthesizeMouseAtCenter(blockedEmbedToggle.buttonEl, {});

  // Wait for smartblock embed script to finish
  await smartblockScriptFinished;

  ok(!blockedEmbedToggle.pressed, "Unblock toggle should be off");

  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    // Check that the "embed" was replaced with a placeholder
    let placeholder = content.document.querySelector(
      ".shimmed-embedded-content"
    );

    ok(placeholder, "Embed replaced with a placeholder after reblock");
  });

  // Check toggle telemetry is triggered
  toggleEvents =
    Glean.securityUiProtectionspopup.clickSmartblockembedsToggle.testGetValue();
  is(toggleEvents.length, 2, "Telemetry triggered for toggle press");
  is(
    toggleEvents[1].extra.isBlock,
    "true",
    "Toggle press telemetry is a block"
  );
  // Note: the openingReason shows as undefined since the test opened the
  // protections panel directly with a function call"
  is(
    toggleEvents[1].extra.openingReason,
    undefined,
    "Smartblock shown event has correct reason"
  );

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_smartblock_click_while_panel_open() {
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

  // Click placeholder button once to open panel
  await clickOnPagePlaceholder(tab);

  // Click again to close panel
  // Discard promise waiting for protections panel open because panel will close
  clickOnPagePlaceholder(tab);

  // Click again to open panel immediately after
  // If this waits infinitely that means panel failed to open on the second click (bad)
  await clickOnPagePlaceholder(tab);

  await BrowserTestUtils.removeTab(tab);
});
