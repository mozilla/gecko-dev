/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test verifies that the url classifier allow list category prefs
 * migration is correctly applied.
 */

let exceptionListService = Cc[
  "@mozilla.org/url-classifier/exception-list-service;1"
].getService(Ci.nsIUrlClassifierExceptionListService);

const ALLOW_LIST_CATEGORY_MIGRATION_PREF =
  "privacy.trackingprotection.allow_list.hasMigratedCategoryPrefs";
const CONTENT_BLOCKING_CATEGORY_PREF = "browser.contentblocking.category";
const BASELINE_ALLOW_LIST_PREF =
  "privacy.trackingprotection.allow_list.baseline.enabled";
const CONVENIENCE_ALLOW_LIST_PREF =
  "privacy.trackingprotection.allow_list.convenience.enabled";

/**
 * Clean up all test preferences to avoid affecting other tests
 */
function cleanupTestPrefs() {
  Services.prefs.clearUserPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF);
  Services.prefs.clearUserPref(CONTENT_BLOCKING_CATEGORY_PREF);
  Services.prefs.clearUserPref(BASELINE_ALLOW_LIST_PREF);
  Services.prefs.clearUserPref(CONVENIENCE_ALLOW_LIST_PREF);
}

add_task(async function test_migration_already_completed() {
  info("Test Branch 1: Migration already completed");
  // Set up the migration pref to true
  Services.prefs.setBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, true);

  // Set up some test values for the allow list prefs.
  // These are the default states.
  Services.prefs.setBoolPref(BASELINE_ALLOW_LIST_PREF, true);
  Services.prefs.setBoolPref(CONVENIENCE_ALLOW_LIST_PREF, true);

  info("Triggering migration with migration pref already set to true");
  exceptionListService.testRunCategoryPrefsMigration();

  info(
    "Verifying that migration pref remains true and allow list prefs unchanged"
  );
  // Verify that the migration pref is still true and allow list prefs unchanged
  ok(
    Services.prefs.getBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, false),
    "Migration pref should remain true when already migrated"
  );
  ok(
    Services.prefs.getBoolPref(BASELINE_ALLOW_LIST_PREF, false),
    "Baseline allow list pref should remain unchanged when already migrated"
  );
  ok(
    Services.prefs.getBoolPref(CONVENIENCE_ALLOW_LIST_PREF, false),
    "Convenience allow list pref should remain unchanged when already migrated"
  );

  // Clean up
  cleanupTestPrefs();
});

add_task(async function test_migration_standard_category() {
  info("Test Branch 2: Content blocking category is 'standard'");
  // Set up the migration pref to false (not migrated yet)
  Services.prefs.setBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, false);

  // Set content blocking category to "standard"
  Services.prefs.setStringPref(CONTENT_BLOCKING_CATEGORY_PREF, "standard");

  // Enable both lists (default state).
  Services.prefs.setBoolPref(BASELINE_ALLOW_LIST_PREF, true);
  Services.prefs.setBoolPref(CONVENIENCE_ALLOW_LIST_PREF, true);

  info("Triggering migration with content blocking category set to 'standard'");
  exceptionListService.testRunCategoryPrefsMigration();

  info("Verifying that migration pref is true and allow list prefs unchanged");
  // Verify that the migration pref is true and allow list prefs unchanged
  ok(
    Services.prefs.getBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, false),
    "Migration pref should be set to true when content blocking category is standard"
  );
  ok(
    Services.prefs.getBoolPref(BASELINE_ALLOW_LIST_PREF, false),
    "Baseline allow list pref should remain unchanged when content blocking category is standard"
  );
  ok(
    Services.prefs.getBoolPref(CONVENIENCE_ALLOW_LIST_PREF, false),
    "Convenience allow list pref should remain unchanged when content blocking category is standard"
  );

  // Clean up
  cleanupTestPrefs();
});

add_task(async function test_migration_strict_category() {
  info("Test Branch 3: Content blocking category is 'strict'");
  // Set up the migration pref to false (not migrated yet)
  Services.prefs.setBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, false);

  // Set content blocking category to "strict"
  Services.prefs.setStringPref(CONTENT_BLOCKING_CATEGORY_PREF, "strict");

  // Enable both lists (default state).
  Services.prefs.setBoolPref(BASELINE_ALLOW_LIST_PREF, true);
  Services.prefs.setBoolPref(CONVENIENCE_ALLOW_LIST_PREF, true);

  info("Triggering migration with content blocking category set to 'strict'");
  // Trigger migration
  exceptionListService.testRunCategoryPrefsMigration();

  info(
    "Verifying that migration pref is set to true and allow list prefs are disabled"
  );
  // Verify that the migration pref is now true and allow list prefs are disabled
  ok(
    Services.prefs.getBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, false),
    "Migration pref should be set to true when content blocking category is strict"
  );
  ok(
    !Services.prefs.getBoolPref(BASELINE_ALLOW_LIST_PREF, true),
    "Baseline allow list pref should be disabled when content blocking category is strict"
  );
  ok(
    !Services.prefs.getBoolPref(CONVENIENCE_ALLOW_LIST_PREF, true),
    "Convenience allow list pref should be disabled when content blocking category is strict"
  );

  // Clean up
  cleanupTestPrefs();
});

add_task(async function test_migration_custom_category() {
  info("Test Branch 3: Content blocking category is 'custom'");
  // Set up the migration pref to false (not migrated yet)
  Services.prefs.setBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, false);

  // Set content blocking category to "custom"
  Services.prefs.setStringPref(CONTENT_BLOCKING_CATEGORY_PREF, "custom");

  // Enable both lists (default state).
  Services.prefs.setBoolPref(BASELINE_ALLOW_LIST_PREF, true);
  Services.prefs.setBoolPref(CONVENIENCE_ALLOW_LIST_PREF, true);

  info("Triggering migration with content blocking category set to 'custom'");
  // Trigger migration
  exceptionListService.testRunCategoryPrefsMigration();

  info(
    "Verifying that migration pref is set to true and allow list prefs remain disabled"
  );
  // Verify that the migration pref is now true and allow list prefs got disabled.
  ok(
    Services.prefs.getBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, false),
    "Migration pref should be set to true when content blocking category is custom"
  );
  ok(
    !Services.prefs.getBoolPref(BASELINE_ALLOW_LIST_PREF, true),
    "Baseline allow list pref should be disabled when content blocking category is custom"
  );
  ok(
    !Services.prefs.getBoolPref(CONVENIENCE_ALLOW_LIST_PREF, true),
    "Convenience allow list pref should be disabled when content blocking category is custom"
  );

  // Clean up
  cleanupTestPrefs();
});

add_task(async function test_migration_mixed_pref_states() {
  info(
    "Test Branch 3: Content blocking category is 'strict' with mixed pref states"
  );
  // Set up the migration pref to false (not migrated yet)
  Services.prefs.setBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, false);

  // Set content blocking category to "strict"
  Services.prefs.setStringPref(CONTENT_BLOCKING_CATEGORY_PREF, "strict");

  // Set up mixed test values for the allow list prefs
  Services.prefs.setBoolPref(BASELINE_ALLOW_LIST_PREF, true);
  Services.prefs.setBoolPref(CONVENIENCE_ALLOW_LIST_PREF, false);

  info(
    "Triggering migration with mixed allow list pref states (baseline=true, convenience=false)"
  );
  // Trigger migration
  exceptionListService.testRunCategoryPrefsMigration();

  info(
    "Verifying that migration pref is set to true and both allow list prefs are disabled"
  );
  // Verify that the migration pref is now true and both allow list prefs are disabled
  ok(
    Services.prefs.getBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, false),
    "Migration pref should be set to true when content blocking category is strict"
  );
  ok(
    !Services.prefs.getBoolPref(BASELINE_ALLOW_LIST_PREF, true),
    "Baseline allow list pref should be disabled regardless of initial state"
  );
  ok(
    !Services.prefs.getBoolPref(CONVENIENCE_ALLOW_LIST_PREF, true),
    "Convenience allow list pref should be disabled regardless of initial state"
  );

  // Clean up
  cleanupTestPrefs();
});

add_task(async function test_migration_default_pref_values() {
  info("Test with default pref values (no prefs set)");
  // Clear any existing prefs
  cleanupTestPrefs();

  info(
    "Triggering migration with default content blocking category ('standard')"
  );
  // Trigger migration
  exceptionListService.testRunCategoryPrefsMigration();

  info(
    "Verifying that migration pref is set to true and allow list prefs use default values"
  );
  // Verify that the migration pref is true and allow list prefs unchanged
  ok(
    Services.prefs.getBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, false),
    "Migration pref should be set to true when content blocking category is standard"
  );
  ok(
    Services.prefs.getBoolPref(BASELINE_ALLOW_LIST_PREF, false),
    "Baseline allow list pref should remain unchanged when content blocking category is standard"
  );
  ok(
    Services.prefs.getBoolPref(CONVENIENCE_ALLOW_LIST_PREF, false),
    "Convenience allow list pref should remain unchanged when content blocking category is standard"
  );
});

add_task(async function test_migration_idempotency() {
  info(
    "Test that migration is idempotent - running it multiple times should not change behavior"
  );
  // Set up the migration pref to false (not migrated yet)
  Services.prefs.setBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, false);

  // Set content blocking category to "strict"
  Services.prefs.setStringPref(CONTENT_BLOCKING_CATEGORY_PREF, "strict");

  // Set up test values for the allow list prefs
  Services.prefs.setBoolPref(BASELINE_ALLOW_LIST_PREF, true);
  Services.prefs.setBoolPref(CONVENIENCE_ALLOW_LIST_PREF, true);

  info("Triggering migration multiple times to test idempotency");
  // Trigger migration multiple times
  exceptionListService.testRunCategoryPrefsMigration();
  exceptionListService.testRunCategoryPrefsMigration();

  info(
    "Verifying that migration pref is set to true and allow list prefs are disabled after multiple calls"
  );
  // Verify that the migration pref is now true and allow list prefs are disabled
  ok(
    Services.prefs.getBoolPref(ALLOW_LIST_CATEGORY_MIGRATION_PREF, false),
    "Migration pref should be set to true after multiple calls"
  );
  ok(
    !Services.prefs.getBoolPref(BASELINE_ALLOW_LIST_PREF, true),
    "Baseline allow list pref should be disabled after multiple calls"
  );
  ok(
    !Services.prefs.getBoolPref(CONVENIENCE_ALLOW_LIST_PREF, true),
    "Convenience allow list pref should be disabled after multiple calls"
  );

  // Clean up
  cleanupTestPrefs();
});
