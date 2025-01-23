/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { FilterAdult } = ChromeUtils.importESModule(
  "resource:///modules/FilterAdult.sys.mjs"
);

let originalPrefValue;
const FILTER_ADULT_ENABLED_PREF =
  "browser.newtabpage.activity-stream.filterAdult";
const TEST_ADULT_SITE_URL = "https://some-adult-site.com/";

add_setup(async function setup() {
  // Save the original preference value to restore later
  originalPrefValue = Services.prefs.getBoolPref(
    FILTER_ADULT_ENABLED_PREF,
    true
  );
  // Enable the filter preference for testing
  Services.prefs.setBoolPref(FILTER_ADULT_ENABLED_PREF, true);
  registerCleanupFunction(() => {
    // Restore the original preference value after all tests
    Services.prefs.setBoolPref(FILTER_ADULT_ENABLED_PREF, originalPrefValue);
  });
});

add_task(async function test_defaults_to_include_unexpected_urls() {
  const empty = {};
  const result = FilterAdult.filter([empty]);
  Assert.equal(result.length, 1);
  Assert.equal(result[0], empty);
});

add_task(async function test_does_not_filter_non_adult_urls() {
  const link = { url: "https://mozilla.org/" };
  const result = FilterAdult.filter([link]);
  Assert.equal(result.length, 1);
  Assert.equal(result[0], link);
});

add_task(async function test_filters_out_adult_urls() {
  // Add a domain to the adult list
  FilterAdult.addDomainToList(TEST_ADULT_SITE_URL);
  const link = { url: TEST_ADULT_SITE_URL };
  const result = FilterAdult.filter([link]);
  Assert.equal(result.length, 0);

  // Clean up
  FilterAdult.removeDomainFromList(TEST_ADULT_SITE_URL);
});

add_task(async function test_does_not_filter_adult_urls_when_pref_off() {
  // Disable the filter preference
  Services.prefs.setBoolPref(FILTER_ADULT_ENABLED_PREF, false);

  // Add a domain to the adult list
  FilterAdult.addDomainToList(TEST_ADULT_SITE_URL);
  const link = { url: TEST_ADULT_SITE_URL };
  const result = FilterAdult.filter([link]);
  Assert.equal(result.length, 1);
  Assert.equal(result[0], link);

  // Re-enable the preference
  Services.prefs.setBoolPref(FILTER_ADULT_ENABLED_PREF, true);

  // Clean up
  FilterAdult.removeDomainFromList(TEST_ADULT_SITE_URL);
});

add_task(async function test_isAdultUrl_returns_false_for_unexpected_urls() {
  const result = FilterAdult.isAdultUrl("");
  Assert.equal(result, false);
});

add_task(async function test_isAdultUrl_returns_false_for_non_adult_urls() {
  const result = FilterAdult.isAdultUrl("https://mozilla.org/");
  Assert.equal(result, false);
});

add_task(async function test_isAdultUrl_returns_true_for_adult_urls() {
  // Add a domain to the adult list
  FilterAdult.addDomainToList(TEST_ADULT_SITE_URL);
  const result = FilterAdult.isAdultUrl(TEST_ADULT_SITE_URL);
  Assert.equal(result, true);

  // Clean up
  FilterAdult.removeDomainFromList(TEST_ADULT_SITE_URL);
});

add_task(async function test_isAdultUrl_returns_false_when_pref_off() {
  // Disable the filter preference
  Services.prefs.setBoolPref(FILTER_ADULT_ENABLED_PREF, false);

  // Add a domain to the adult list
  FilterAdult.addDomainToList(TEST_ADULT_SITE_URL);
  const result = FilterAdult.isAdultUrl(TEST_ADULT_SITE_URL);
  Assert.equal(result, false);

  // Re-enable the preference
  Services.prefs.setBoolPref(FILTER_ADULT_ENABLED_PREF, true);

  // Clean up
  FilterAdult.removeDomainFromList(TEST_ADULT_SITE_URL);
});

add_task(async function test_add_and_remove_from_adult_list() {
  // Add a domain to the adult list
  FilterAdult.addDomainToList(TEST_ADULT_SITE_URL);
  let result = FilterAdult.isAdultUrl(TEST_ADULT_SITE_URL);
  Assert.equal(result, true);

  // Remove the domain from the list
  FilterAdult.removeDomainFromList(TEST_ADULT_SITE_URL);
  result = FilterAdult.isAdultUrl(TEST_ADULT_SITE_URL);
  Assert.equal(result, false);
});
