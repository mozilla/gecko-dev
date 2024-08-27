/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// TODO: Test fallback to normal default when no private set at all.

"use strict";

const CONFIG = [{ identifier: "appDefault" }, { identifier: "other" }];

add_task(async function test_no_private_default_falls_back_to_normal_default() {
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );
  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");

  await Services.search.init();

  Assert.ok(Services.search.isInitialized, "search initialized");

  Assert.equal(
    Services.search.appDefaultEngine.name,
    "appDefault",
    "Should have the expected engine as app default"
  );
  Assert.equal(
    Services.search.defaultEngine.name,
    "appDefault",
    "Should have the expected engine as default"
  );
  Assert.equal(
    Services.search.appPrivateDefaultEngine.name,
    "appDefault",
    "Should have the same engine for the app private default"
  );
  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "appDefault",
    "Should have the same engine for the private default"
  );
});
