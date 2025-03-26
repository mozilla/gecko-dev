/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the submitHealthReportBox checkbox is automatically updated when the
// corresponding datareporting.healthreport.uploadEnabled pref is changed.

const PREF_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

add_task(async function test_updatePageFromPref() {
  if (!AppConstants.MOZ_TELEMETRY_REPORTING) {
    ok(true, "Skipping test because telemetry reporting is disabled");
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [[PREF_UPLOAD_ENABLED, false]],
  });

  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });

  const doc = gBrowser.selectedBrowser.contentDocument;
  const checkbox = doc.getElementById("submitHealthReportBox");
  Assert.ok(!checkbox.checked, "checkbox should match pref state on page load");

  await SpecialPowers.pushPrefEnv({
    set: [[PREF_UPLOAD_ENABLED, true]],
  });

  let checkboxUpdated = BrowserTestUtils.waitForMutationCondition(
    checkbox,
    { attributeFilter: ["checked"] },
    () => checkbox.checked
  );
  await checkboxUpdated;

  Assert.ok(checkbox.checked, "pref change should trigger checkbox update");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
