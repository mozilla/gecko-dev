/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Hide protections cards so as not to trigger more async messaging
      // when landing on the page.
      ["browser.contentblocking.report.monitor.enabled", false],
      ["browser.contentblocking.report.lockwise.enabled", false],
      ["browser.contentblocking.report.proxy.enabled", false],
    ],
  });
});

add_task(async function doTest() {
  // This also ensures that the DB tables have been initialized.
  await TrackingDBService.clearAll();

  let milestones = JSON.parse(
    Services.prefs.getStringPref(
      "browser.contentblocking.cfr-milestone.milestones"
    )
  );
  let totalTrackerCount = 0;

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );

  for (let milestone of milestones) {
    let trackerCount = milestone - totalTrackerCount;
    await addTrackerDataIntoDB(trackerCount);
    totalTrackerCount += trackerCount;

    // Trigger the milestone feature.
    await TrackingDBService.saveEvents("{}");

    await TestUtils.waitForCondition(
      () => gProtectionsHandler._milestoneTextSet
    );

    // We set the shown-time pref to pretend that the CFR has been
    // shown, so that we can test the panel.
    // TODO: Full integration test for robustness.
    Services.prefs.setStringPref(
      "browser.contentblocking.cfr-milestone.milestone-shown-time",
      Date.now().toString()
    );

    await openProtectionsPanel();

    ok(
      BrowserTestUtils.is_visible(
        gProtectionsHandler._protectionsPopupMilestonesText
      ),
      "Milestones section should be visible in the panel."
    );

    await closeProtectionsPanel();
    await openProtectionsPanel();

    ok(
      BrowserTestUtils.is_visible(
        gProtectionsHandler._protectionsPopupMilestonesText
      ),
      "Milestones section should still be visible in the panel."
    );

    let newTabPromise = waitForAboutProtectionsTab();
    await EventUtils.synthesizeMouseAtCenter(
      document.getElementById("protections-popup-milestones-content"),
      {}
    );
    let protectionsTab = await newTabPromise;

    ok(true, "about:protections has been opened as expected.");

    BrowserTestUtils.removeTab(protectionsTab);

    await openProtectionsPanel();

    ok(
      !BrowserTestUtils.is_visible(
        gProtectionsHandler._protectionsPopupMilestonesText
      ),
      "Milestones section should no longer be visible in the panel."
    );

    await closeProtectionsPanel();
  }

  BrowserTestUtils.removeTab(tab);
  await TrackingDBService.clearAll();
});
