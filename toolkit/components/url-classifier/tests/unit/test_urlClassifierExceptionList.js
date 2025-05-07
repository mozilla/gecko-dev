/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the nsIUrlClassifierExceptionList interface.
 */

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
    urlPattern,
    topLevelUrlPattern = "",
    isPrivateBrowsingOnly = false,
    filterContentBlockingCategories = [],
    classifierFeatures = [],
  } = rsObject;

  entry.init(
    urlPattern,
    topLevelUrlPattern,
    isPrivateBrowsingOnly,
    filterContentBlockingCategories,
    classifierFeatures
  );

  return entry;
}

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
      urlPattern: "*://tracker.com/*",
      topLevelUrlPattern: "*://example.com/*",
    })
  );

  // An entry that allow-lists *.sub.tracker.com under *.example.org.
  list.addEntry(
    rsObjectToEntry({
      urlPattern: "*://*.sub.tracker.com/*",
      topLevelUrlPattern: "*://*.example.org/*",
    })
  );

  // An entry that allow-lists tracker.org globally.
  list.addEntry(
    rsObjectToEntry({
      urlPattern: "*://tracker.org/*",
    })
  );
  // An entry that allow-lists foo.tracker.org globally.
  list.addEntry(
    rsObjectToEntry({
      urlPattern: "*://foo.tracker.org/*",
    })
  );
  // An entry that allow-lists foo.bar.tracker.org globally.
  list.addEntry(
    rsObjectToEntry({
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
