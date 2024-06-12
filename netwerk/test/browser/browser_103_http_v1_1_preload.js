/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.prefs.setBoolPref("network.early-hints.enabled", true);
Services.prefs.setBoolPref("network.early-hints.over-http-v1-1.enabled", true);

// Disable mixed-content upgrading as this test is expecting HTTP image loads
Services.prefs.setBoolPref(
  "security.mixed_content.upgrade_display_content",
  false
);

const { request_count_checking, test_preload_url, test_hint_preload } =
  ChromeUtils.importESModule(
    "resource://testing-common/early_hint_preload_test_helper.sys.mjs"
  );

// Test that with both early hints and early hints over http v1-1 prefs are disabled,
// no early hint requests are made
add_task(async function test_103_both_preload_disabled() {
  Services.prefs.setBoolPref("network.early-hints.enabled", false);
  Services.prefs.setBoolPref(
    "network.early-hints.over-http-v1-1.enabled",
    false
  );
  await test_hint_preload(
    "test_103_preload_disabled",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 0, normal: 1 }
  );
  Services.prefs.setBoolPref("network.early-hints.enabled", true);
  Services.prefs.setBoolPref(
    "network.early-hints.over-http-v1-1.enabled",
    true
  );
});

// Test that with only early hints over http v1-1 config option is disabled, no early hint requests are made
add_task(async function test_103_http_v1_1_preload_disabled() {
  Services.prefs.setBoolPref("network.early-hints.enabled", true);
  Services.prefs.setBoolPref("network.early-hints.enabled", false);
  await test_hint_preload(
    "test_103_preload_disabled",
    "https://example.com",
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel.sjs",
    { hinted: 0, normal: 1 }
  );
  Services.prefs.setBoolPref("network.early-hints.enabled", true);
  Services.prefs.setBoolPref(
    "network.early-hints.over-http-v1-1.enabled",
    true
  );
});
