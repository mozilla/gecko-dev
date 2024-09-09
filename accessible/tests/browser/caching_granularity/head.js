/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported verifyAttributeCached, verifyAttributeCachedNoRetry,
            testAttributeCachePresence, testCachingPerPlatform */

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as events.js.
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "layout.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR }
);

/**
 * Verifies that the given attribute is cached on the given acc. Retries until
 * a timeout via untilCacheOk.
 * @param {nsIAccessible} accessible the accessible where the attribute to query
 *                                   should be cached
 * @param {String}        attribute  the attribute to query in the cache
 */
async function verifyAttributeCached(accessible, attribute) {
  // Wait until the attribute is present in the cache.
  await untilCacheOk(() => {
    try {
      accessible.cache.getStringProperty(attribute);
      return true;
    } catch (e) {
      return false;
    }
  }, `${attribute} is present in the cache`);
}

/**
 * Verifies that the given attribute is cached on the given acc. Doesn't retry
 * until a timeout.
 * @param {nsIAccessible} accessible the accessible where the attribute to query
 *                                   should be cached
 * @param {String}        attribute  the attribute to query in the cache
 */
function verifyAttributeCachedNoRetry(accessible, attribute) {
  try {
    accessible.cache.getStringProperty(attribute);
    ok(true, `${attribute} is present in the cache`);
  } catch (e) {
    ok(false, `${attribute} is not present in the cache`);
  }
}

/*
 * @callback QueryCallback A function taking no arguments that queries an
 *                         attribute that may be cached, e.g., bounds, state
 */

/**
 * Verifies that the given attribute isn't cached. Then, forces the
 * accessibility service to activate those cache domains by running the provided
 * query function, which queries the attribute. Finally, verifies that the
 * attribute is present in the cache.
 * @param  {nsIAccessible}  accessible the accessible where the attribute to
 *                                     query should be cached
 * @param  {String}         attribute  the attribute to query in the cache
 * @param  {QueryCallback}  queryCb    the callback that this function will
 *                                     invoke to query the given attribute
 */
async function testAttributeCachePresence(accessible, attribute, queryCb) {
  // Verify that the attribute isn't cached.
  let hasAttribute;
  try {
    accessible.cache.getStringProperty(attribute);
    hasAttribute = true;
  } catch (e) {
    hasAttribute = false;
  }
  ok(!hasAttribute, `${attribute} is not present in cache`);

  // Force attribute to be cached by querying it.
  info(`Querying ${attribute} in cache`);
  queryCb();

  // Wait until the attribute is present in the cache.
  await verifyAttributeCached(accessible, attribute);
}

/**
 * Verify that the given attribute is properly cached, taking into account
 * platform considerations which may affect what is testable. Ideally, test
 * attribute absence and presence, but only presence may be possible.
 * @param  {nsIAccessible}  accessible the accessible where the attribute to
 *                                     query should be cached
 * @param  {String}         attribute  the attribute to query in the cache
 * @param  {QueryCallback}  queryCb    the callback that this function will
 *                                     invoke to query the given attribute
 */
async function testCachingPerPlatform(accessible, attribute, queryCb) {
  // On Linux, the implementation of PlatformEvent for EVENT_NAME_CHANGE calls
  // RemoteAccessible::Name during the test setup, which unavoidably queries the
  // Text cache domain. Therefore, on Linux we avoid checking for the absence of
  // the Text domain attributes. Similarly, we cache the viewport on Linux
  // before a test is ready to run.
  if (
    AppConstants.platform == "linux" &&
    (attribute == "language" ||
      attribute == "text" ||
      attribute == "style" ||
      attribute == "viewport")
  ) {
    await verifyAttributeCached(accessible, attribute);
  } else if (
    AppConstants.platform == "macosx" &&
    (attribute == "headers" ||
      attribute == "colspan" ||
      attribute == "rowspan" ||
      attribute == "layout-guess" ||
      attribute == "language" ||
      attribute == "text" ||
      attribute == "style")
  ) {
    // These attributes work on macOS, but aren't consistent. Events may happen
    // before document load complete that cause caching before the test starts.
    // So, in the (common) event that that doesn't happen, call the queryCb to
    // ensure the necessary cache request happens. See bug 1916578.
    queryCb();
    await verifyAttributeCached(accessible, attribute);
  } else {
    await testAttributeCachePresence(accessible, attribute, queryCb);
  }
}
