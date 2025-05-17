/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { _ContextId } = ChromeUtils.importESModule(
  "moz-src:///browser/modules/ContextId.sys.mjs"
);

const CONTEXT_ID_PREF = "browser.contextual-services.contextId";
const CONTEXT_ID_TIMESTAMP_PREF =
  "browser.contextual-services.contextId.timestamp-in-seconds";
const CONTEXT_ID_ROTATION_DAYS_PREF =
  "browser.contextual-services.contextId.rotation-in-days";
const UUID_WITH_BRACES_REGEX =
  /^\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}$/i;
const TEST_CONTEXT_ID_WITH_BRACES = "{decafbad-0cd1-0cd2-0cd3-decafbad1000}";

do_get_profile();

/**
 * Test that if there's a pre-existing contextID, we can get it, and that a
 * timestamp will be generated for it.
 */
add_task(async function test_get_existing() {
  Services.prefs.setCharPref(CONTEXT_ID_PREF, TEST_CONTEXT_ID_WITH_BRACES);
  Services.prefs.clearUserPref(CONTEXT_ID_TIMESTAMP_PREF);
  Services.prefs.setIntPref(CONTEXT_ID_ROTATION_DAYS_PREF, 0);

  let ContextId = new _ContextId();

  Assert.equal(
    await ContextId.request(),
    TEST_CONTEXT_ID_WITH_BRACES,
    "Should have gotten the stored context ID"
  );

  Assert.ok(
    !Services.prefs.prefHasUserValue(CONTEXT_ID_TIMESTAMP_PREF),
    "No timestamp was persisted."
  );
  Assert.equal(
    Services.prefs.getCharPref(CONTEXT_ID_PREF),
    TEST_CONTEXT_ID_WITH_BRACES,
    "The same context ID is still stored after requesting."
  );

  Assert.equal(
    await ContextId.request(),
    TEST_CONTEXT_ID_WITH_BRACES,
    "Should have gotten the same stored context ID back again."
  );

  // We should be able to synchronously request the context ID in this
  // configuration.
  Assert.equal(
    ContextId.requestSynchronously(),
    TEST_CONTEXT_ID_WITH_BRACES,
    "Got the stored context ID back synchronously."
  );
});

/**
 * Test that if there's not a pre-existing contextID, we will generate one, but
 * no timestamp will be generated for it.
 */
add_task(async function test_generate() {
  Services.prefs.clearUserPref(CONTEXT_ID_PREF);
  Services.prefs.clearUserPref(CONTEXT_ID_TIMESTAMP_PREF);

  let ContextId = new _ContextId();

  const generatedContextID = await ContextId.request();

  Assert.ok(
    UUID_WITH_BRACES_REGEX.test(generatedContextID),
    "Should have gotten a UUID generated for the context ID."
  );

  Assert.ok(
    !Services.prefs.prefHasUserValue(CONTEXT_ID_TIMESTAMP_PREF),
    "No timestamp was persisted."
  );

  Assert.equal(
    await ContextId.request(),
    generatedContextID,
    "Should have gotten the same stored context ID back again."
  );

  // We should be able to synchronously request the context ID in this
  // configuration.
  Assert.equal(
    ContextId.requestSynchronously(),
    generatedContextID,
    "Got the stored context ID back synchronously."
  );
});

/**
 * Test that if we have a pre-existing context ID, and we (for some reason)
 * have rotation period set to a non-zero value, and a creation timestamp
 * exists, that the context ID does not rotate between requests.
 */
add_task(async function test_no_rotation() {
  Services.prefs.setCharPref(CONTEXT_ID_PREF, TEST_CONTEXT_ID_WITH_BRACES);
  Services.prefs.setIntPref(CONTEXT_ID_TIMESTAMP_PREF, 1);
  Services.prefs.setIntPref(CONTEXT_ID_ROTATION_DAYS_PREF, 1);
  // Let's say there's a 30 day rotation window.
  const ROTATION_DAYS = 30;
  Services.prefs.setIntPref(CONTEXT_ID_ROTATION_DAYS_PREF, ROTATION_DAYS);

  let ContextId = new _ContextId();

  Assert.ok(
    !ContextId.rotationEnabled,
    "ContextId should report that rotation is not enabled."
  );

  Assert.equal(
    await ContextId.request(),
    TEST_CONTEXT_ID_WITH_BRACES,
    "Should have gotten the stored context ID"
  );
  Assert.equal(
    Services.prefs.getIntPref(CONTEXT_ID_TIMESTAMP_PREF),
    1,
    "The timestamp should not have changed."
  );

  // We should be able to synchronously request the context ID in this
  // configuration.
  Assert.equal(
    ContextId.requestSynchronously(),
    TEST_CONTEXT_ID_WITH_BRACES,
    "Got the stored context ID back synchronously."
  );
});

/**
 * Test that calling forceRotation is a no-op with the legacy backend.
 */
add_task(async function test_force_rotation() {
  Services.prefs.setCharPref(CONTEXT_ID_PREF, TEST_CONTEXT_ID_WITH_BRACES);
  Services.prefs.clearUserPref(CONTEXT_ID_TIMESTAMP_PREF);

  let ContextId = new _ContextId();
  Assert.equal(
    await ContextId.request(),
    TEST_CONTEXT_ID_WITH_BRACES,
    "Should have gotten the stored context ID"
  );

  await ContextId.forceRotation();

  Assert.equal(
    await ContextId.request(),
    TEST_CONTEXT_ID_WITH_BRACES,
    "Should have gotten the stored context ID"
  );
  Assert.ok(
    !Services.prefs.prefHasUserValue(CONTEXT_ID_TIMESTAMP_PREF),
    "The timestamp should not have changed."
  );

  // We should be able to synchronously request the context ID in this
  // configuration.
  Assert.equal(
    ContextId.requestSynchronously(),
    TEST_CONTEXT_ID_WITH_BRACES,
    "Got the stored context ID back synchronously."
  );
});
