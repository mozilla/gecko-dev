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

add_task(test_sync_reloading_extensions_works);

add_task(async function test_storage_sync() {
  await test_background_page_storage("sync");
});

add_task(test_storage_sync_requires_real_id);

add_task(async function test_bytes_in_use() {
  await test_background_storage_area_with_bytes_in_use("sync", true);
});

add_task(async function test_storage_onChanged_event_page() {
  await test_storage_change_event_page("sync");
});
