/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * This file contains tests for the "Add Engine" subdialog.
 */

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update2.engineAliasRefresh", true]],
  });
});

/**
 * Test for searching for the "Add Engine" subdialog.
 */
add_task(async function searchAddEngine() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  await evaluateSearchResults("Add Engine", "oneClickSearchProvidersGroup");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
