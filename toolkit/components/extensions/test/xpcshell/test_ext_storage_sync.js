/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.prefs.setBoolPref("webextensions.storage.sync.kinto", false);

AddonTestUtils.init(this);

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();

  // FOG needs a profile directory to put its data in.
  do_get_profile();
  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});

add_task(test_config_flag_needed);

add_task(test_sync_reloading_extensions_works);

add_task(function test_storage_sync() {
  return runWithPrefs([[STORAGE_SYNC_PREF, true]], () =>
    test_background_page_storage("sync")
  );
});

add_task(test_storage_sync_requires_real_id);

add_task(function test_bytes_in_use() {
  return runWithPrefs([[STORAGE_SYNC_PREF, true]], () =>
    test_background_storage_area_with_bytes_in_use("sync", true)
  );
});

add_task(function test_storage_onChanged_event_page() {
  return runWithPrefs([[STORAGE_SYNC_PREF, true]], () =>
    test_storage_change_event_page("sync")
  );
});

add_task(async function test_storage_sync_telemetry() {
  return runWithPrefs([[STORAGE_SYNC_PREF, true]], () =>
    test_storage_sync_telemetry_quota("rust", true)
  );
});
