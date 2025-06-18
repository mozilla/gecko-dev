/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the nsIUrlClassifierExceptionList interface.
 */

const ALLOW_LIST_BASELINE_PREF =
  "privacy.trackingprotection.allow_list.baseline.enabled";
const ALLOW_LIST_CONVENIENCE_PREF =
  "privacy.trackingprotection.allow_list.convenience.enabled";

/**
 * Convert a JS object from RemoteSettings to an nsIUrlClassifierExceptionListEntry.
 * Copied from UrlClassifierExceptionListService.sys.mjs with modifications.
 * @param {Object} rsObject - The JS object from RemoteSettings to convert.
 * @returns {nsIUrlClassifierExceptionListEntry} The converted nsIUrlClassifierExceptionListEntry.
 */
function rsObjectToEntry(rsObject) {
  let entry = Cc[
    "@mozilla.org/url-classifier/exception-list-entry;1"
  ].createInstance(Ci.nsIUrlClassifierExceptionListEntry);

  let {
    category: categoryStr,
    urlPattern,
    topLevelUrlPattern = "",
    isPrivateBrowsingOnly = false,
    filterContentBlockingCategories = [],
    classifierFeatures = [],
  } = rsObject;

  const CATEGORY_STR_TO_ENUM = {
    "internal-pref":
      Ci.nsIUrlClassifierExceptionListEntry.CATEGORY_INTERNAL_PREF,
    baseline: Ci.nsIUrlClassifierExceptionListEntry.CATEGORY_BASELINE,
    convenience: Ci.nsIUrlClassifierExceptionListEntry.CATEGORY_CONVENIENCE,
  };

  let category = CATEGORY_STR_TO_ENUM[categoryStr];

  entry.init(
    category,
    urlPattern,
    topLevelUrlPattern,
    isPrivateBrowsingOnly,
    filterContentBlockingCategories,
    classifierFeatures
  );

  return entry;
}

/**
 * Test the exception list with manually imported allow-list entries.
 */
add_task(async function test_exception_list_lookups() {
  let list = Cc["@mozilla.org/url-classifier/exception-list;1"].createInstance(
    Ci.nsIUrlClassifierExceptionList
  );

  Assert.ok(
    !list.matches(
      Services.io.newURI("https://tracker.com/"),
      Services.io.newURI("https://example.com/"),
      false
    ),
    "Exception list with no entries should not match tracker.com"
  );

  // An entry that allow-lists tracker.com under example.com.
  list.addEntry(
    rsObjectToEntry({
      category: "internal-pref",
      urlPattern: "*://tracker.com/*",
      topLevelUrlPattern: "*://example.com/*",
    })
  );

  // An entry that allow-lists *.sub.tracker.com under *.example.org.
  list.addEntry(
    rsObjectToEntry({
      category: "internal-pref",
      urlPattern: "*://*.sub.tracker.com/*",
      topLevelUrlPattern: "*://*.example.org/*",
    })
  );

  // An entry that allow-lists tracker.org globally.
  list.addEntry(
    rsObjectToEntry({
      category: "internal-pref",
      urlPattern: "*://tracker.org/*",
    })
  );
  // An entry that allow-lists foo.tracker.org globally.
  list.addEntry(
    rsObjectToEntry({
      category: "internal-pref",
      urlPattern: "*://foo.tracker.org/*",
    })
  );
  // An entry that allow-lists foo.bar.tracker.org globally.
  list.addEntry(
    rsObjectToEntry({
      category: "internal-pref",
      urlPattern: "*://foo.bar.tracker.org/*",
    })
  );

  Assert.ok(
    list.matches(
      Services.io.newURI("https://tracker.com/bar"),
      Services.io.newURI("https://example.com/foo"),
      false
    ),
    "Exception list should match tracker.com under example.com."
  );

  Assert.ok(
    !list.matches(
      Services.io.newURI("https://tracker.com/bar"),
      Services.io.newURI("https://example.org/foo"),
      false
    ),
    "Exception list should not match tracker.com under example.org."
  );

  Assert.ok(
    list.matches(
      Services.io.newURI("https://foo.sub.tracker.com/bar"),
      Services.io.newURI("https://foo.bar.example.org/foo/bar"),
      false
    ),
    "Exception list should match foo.sub.tracker.com under foo.bar.example.org."
  );

  Assert.ok(
    list.matches(
      Services.io.newURI("https://tracker.org/bar"),
      Services.io.newURI("https://mozilla.org/foo/bar"),
      false
    ),
    "Exception list should match tracker.org under mozilla.org."
  );

  Assert.ok(
    list.matches(
      Services.io.newURI("https://foo.bar.tracker.org/bar"),
      Services.io.newURI("https://mozilla.org/foo/bar"),
      false
    ),
    "Exception list should match foo.bar.tracker.org under mozilla.org."
  );

  Assert.ok(
    !list.matches(
      Services.io.newURI("https://sub.foo.bar.tracker.org/bar"),
      Services.io.newURI("https://mozilla.org/foo/bar"),
      false
    ),
    "Exception list should not match sub.foo.bar.tracker.org under mozilla.org."
  );
});

/**
 * Test the exception list with allow-list categories enabled/disabled via prefs.
 */
add_task(async function test_exception_list_lookups_with_category_prefs() {
  let list = Cc["@mozilla.org/url-classifier/exception-list;1"].createInstance(
    Ci.nsIUrlClassifierExceptionList
  );

  // An entry that allow-lists tracker.com.
  list.addEntry(
    rsObjectToEntry({
      category: "internal-pref",
      urlPattern: "*://tracker.com/*",
    })
  );

  // An entry that allow-lists foo.tracker.com for the baseline category.
  list.addEntry(
    rsObjectToEntry({
      category: "baseline",
      urlPattern: "*://foo.tracker.com/*",
    })
  );

  // An entry that allow-lists bar.tracker.com for the convenience category.
  list.addEntry(
    rsObjectToEntry({
      category: "convenience",
      urlPattern: "*://bar.tracker.com/*",
    })
  );

  info("Start with both allow-list categories disabled.");
  Services.prefs.setBoolPref(ALLOW_LIST_BASELINE_PREF, false);
  Services.prefs.setBoolPref(ALLOW_LIST_CONVENIENCE_PREF, false);

  Assert.ok(
    list.matches(
      Services.io.newURI("https://tracker.com/bar"),
      Services.io.newURI("https://example.org/foo"),
      false
    ),
    "Exception list should match tracker.com when both allow-list categories are disabled."
  );

  Assert.ok(
    !list.matches(
      Services.io.newURI("https://foo.tracker.com/bar"),
      Services.io.newURI("https://example.org/foo"),
      false
    ),
    "Exception list should not match foo.tracker.com because baseline allow-list is disabled."
  );

  Assert.ok(
    !list.matches(
      Services.io.newURI("https://bar.tracker.com/bar"),
      Services.io.newURI("https://example.org/foo"),
      false
    ),
    "Exception list should not match bar.tracker.com because convenience allow-list is disabled."
  );

  info("Enable only the baseline allow-list.");
  Services.prefs.setBoolPref(ALLOW_LIST_BASELINE_PREF, true);
  Services.prefs.setBoolPref(ALLOW_LIST_CONVENIENCE_PREF, false);

  Assert.ok(
    list.matches(
      Services.io.newURI("https://tracker.com/bar"),
      Services.io.newURI("https://example.org/foo"),
      false
    ),
    "Exception list should match tracker.com when only the baseline allow-list is enabled."
  );

  Assert.ok(
    list.matches(
      Services.io.newURI("https://foo.tracker.com/bar"),
      Services.io.newURI("https://example.org/foo"),
      false
    ),
    "Exception list should match foo.tracker.com because baseline allow-list is enabled."
  );

  Assert.ok(
    !list.matches(
      Services.io.newURI("https://bar.tracker.com/bar"),
      Services.io.newURI("https://example.org/foo"),
      false
    ),
    "Exception list should not match bar.tracker.com because convenience allow-list is disabled."
  );

  info("Enable both allow-list categories.");
  Services.prefs.setBoolPref(ALLOW_LIST_BASELINE_PREF, true);
  Services.prefs.setBoolPref(ALLOW_LIST_CONVENIENCE_PREF, true);

  Assert.ok(
    list.matches(
      Services.io.newURI("https://tracker.com/bar"),
      Services.io.newURI("https://example.org/foo"),
      false
    ),
    "Exception list should match tracker.com when both allow-list categories are enabled."
  );

  Assert.ok(
    list.matches(
      Services.io.newURI("https://foo.tracker.com/bar"),
      Services.io.newURI("https://example.org/foo"),
      false
    ),
    "Exception list should match foo.tracker.com because baseline allow-list is enabled."
  );

  Assert.ok(
    list.matches(
      Services.io.newURI("https://bar.tracker.com/bar"),
      Services.io.newURI("https://example.org/foo"),
      false
    ),
    "Exception list should not match bar.tracker.com because convenience allow-list is enabled."
  );

  Services.prefs.clearUserPref(ALLOW_LIST_BASELINE_PREF);
  Services.prefs.clearUserPref(ALLOW_LIST_CONVENIENCE_PREF);
});
