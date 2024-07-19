/*
 * This file contains tests for the Preferences search bar.
 */

/**
 * Test for searching for the "Allowed Sites - Add-ons Installation" subdialog.
 */
add_task(async function () {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  await evaluateSearchResults("allowed to install add-ons", "permissionsGroup");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Certificate Manager" subdialog.
 */
add_task(async function () {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  await evaluateSearchResults(
    "identify these certificate authorities",
    "certSelection"
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
