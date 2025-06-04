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
const UUID_REGEX =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const TEST_CONTEXT_ID = "decafbad-0cd1-0cd2-0cd3-decafbad1000";
const TEST_CONTEXT_ID_WITH_BRACES = "{" + TEST_CONTEXT_ID + "}";
const TOPIC_APP_QUIT = "quit-application";

do_get_profile();

/**
 * Resolves when the passed in ContextId instance fires the ContextId:Persisted
 * event.
 *
 * @param {_ContextId} instance
 *   An instance of the _ContextId class under test.
 * @returns {Promise<CustomEvent>}
 */
function waitForPersist(instance) {
  return new Promise(resolve => {
    instance.addEventListener("ContextId:Persisted", resolve, { once: true });
  });
}

/**
 * Resolves when the the context-id-deletion-request ping is next sent, and
 * checks that it sends the rotatedFromContextId value.
 *
 * @param {string} rotatedFromContextIed
 *   The context ID that was rotated away from.
 * @param {Function} taskFn
 *   A function that will trigger the ping to be sent. This function might
 *   be async.
 * @returns {Promise<undefined>}
 */
function waitForRotated(rotatedFromContextId, taskFn) {
  return GleanPings.contextIdDeletionRequest.testSubmission(() => {
    Assert.equal(
      Glean.contextualServices.contextId.testGetValue(),
      rotatedFromContextId,
      "Sent the right context ID to be deleted."
    );
  }, taskFn);
}

/**
 * Checks that when a taskFn resolves, a context ID rotation has not occurred
 * for the instance.
 *
 * @param {_ContextId} instance
 *   The instance of _ContextId under test.
 * @param {function} taskFn
 *   A function that is being tested to ensure that it does not cause rotation
 *   to occur. It can be async.
 * @returns {Promise<undefined>}
 */
async function doesNotRotate(instance, taskFn) {
  let controller = new AbortController();
  instance.addEventListener(
    "ContextId:Rotated",
    () => {
      Assert.ok(false, "Saw unexpected rotation.");
    },
    { signal: controller.signal }
  );
  await taskFn();
  controller.abort();
}

/**
 * Calls a testing function with a ContextId instance. Once the testing function
 * resolves (or if it throws), this function will take care of cleaning up the
 * instance.
 *
 * @param {function(_ContextId): Promise<undefined>} taskFn
 *   A testing function, which will be passed an instance of _ContextId to run
 *   its test on. The function can be async.
 * @returns {Promise<undefined>}
 */
async function withTestingContextId(taskFn) {
  let instance = new _ContextId();
  try {
    await taskFn(instance);
  } finally {
    instance.observe(null, TOPIC_APP_QUIT, null);
  }
}

add_setup(() => {
  Services.fog.initializeFOG();
  registerCleanupFunction(() => {
    // Importing from ContextId.sys.mjs will have automatically instantiated
    // and registered the default ContextId. We need to inform it that we're
    // shutting down so that it can uninitialiez itself.
    const { ContextId } = ChromeUtils.importESModule(
      "moz-src:///browser/modules/ContextId.sys.mjs"
    );

    ContextId.observe(null, TOPIC_APP_QUIT, null);
  });
});

/**
 * Test that if there's a pre-existing contextID, we can get it, and that a
 * timestamp will be generated for it.
 */
add_task(async function test_get_existing() {
  // Historically, we've stored the context ID with braces, but our endpoints
  // actually would prefer just the raw UUID. The Rust component does the
  // work of stripping those off for us automatically. We'll test that by
  // starting with a context ID with braces in storage, and ensuring that
  // what gets saved and what gets returned does not have braces.
  Services.prefs.setCharPref(CONTEXT_ID_PREF, TEST_CONTEXT_ID_WITH_BRACES);
  Services.prefs.clearUserPref(CONTEXT_ID_TIMESTAMP_PREF);
  Services.prefs.setIntPref(CONTEXT_ID_ROTATION_DAYS_PREF, 0);

  await withTestingContextId(async instance => {
    let persisted = waitForPersist(instance);

    Assert.equal(
      await instance.request(),
      TEST_CONTEXT_ID,
      "Should have gotten the stored context ID"
    );

    await persisted;
    Assert.equal(
      typeof Services.prefs.getIntPref(CONTEXT_ID_TIMESTAMP_PREF, 0),
      "number",
      "We stored a timestamp for the context ID."
    );
    Assert.equal(
      Services.prefs.getCharPref(CONTEXT_ID_PREF),
      TEST_CONTEXT_ID,
      "We stored a the context ID without braces."
    );

    Assert.equal(
      await instance.request(),
      TEST_CONTEXT_ID,
      "Should have gotten the same stored context ID back again."
    );
  });
});

/**
 * Test that if there's not a pre-existing contextID, we will generate one, and
 * a timestamp will be generated for it.
 */
add_task(async function test_generate() {
  Services.prefs.clearUserPref(CONTEXT_ID_PREF);
  Services.prefs.clearUserPref(CONTEXT_ID_TIMESTAMP_PREF);

  await withTestingContextId(async instance => {
    let persisted = waitForPersist(instance);

    const generatedContextID = await instance.request();
    await persisted;

    Assert.ok(
      UUID_REGEX.test(generatedContextID),
      "Should have gotten a UUID generated for the context ID."
    );
    Assert.equal(
      typeof Services.prefs.getIntPref(CONTEXT_ID_TIMESTAMP_PREF, 0),
      "number",
      "We stored a timestamp for the context ID."
    );

    Assert.equal(
      await instance.request(),
      generatedContextID,
      "Should have gotten the same stored context ID back again."
    );
  });
});

/**
 * Test that if we have a pre-existing context ID, with an extremely old
 * creation date (we'll use a creation date of 1, which is back in the 1970s),
 * but a rotation setting of 0, that we don't rotate the context ID.
 */
add_task(async function test_no_rotation() {
  Services.prefs.setCharPref(CONTEXT_ID_PREF, TEST_CONTEXT_ID);
  Services.prefs.setIntPref(CONTEXT_ID_TIMESTAMP_PREF, 1);
  Services.prefs.setIntPref(CONTEXT_ID_ROTATION_DAYS_PREF, 0);

  await withTestingContextId(async instance => {
    Assert.ok(
      !instance.rotationEnabled,
      "ContextId should report that rotation is not enabled."
    );

    await doesNotRotate(instance, async () => {
      Assert.equal(
        await instance.request(),
        TEST_CONTEXT_ID,
        "Should have gotten the stored context ID"
      );
    });

    // We should be able to synchronously request the context ID in this
    // configuration.
    Assert.equal(
      instance.requestSynchronously(),
      TEST_CONTEXT_ID,
      "Got the stored context ID back synchronously."
    );
  });
});

/**
 * Test that if we have a pre-existing context ID, and if the age associated
 * with it is greater than our rotation window, that we'll generate a new
 * context ID and update the creation timestamp. We'll use a creation timestamp
 * of the original context ID of 1, which is sometime in the 1970s.
 */
add_task(async function test_rotation() {
  Services.prefs.setCharPref(CONTEXT_ID_PREF, TEST_CONTEXT_ID);
  Services.prefs.setIntPref(CONTEXT_ID_TIMESTAMP_PREF, 1);
  // Let's say there's a 30 day rotation window.
  const ROTATION_DAYS = 30;
  Services.prefs.setIntPref(CONTEXT_ID_ROTATION_DAYS_PREF, ROTATION_DAYS);

  await withTestingContextId(async instance => {
    Assert.ok(
      instance.rotationEnabled,
      "ContextId should report that rotation is enabled."
    );

    let generatedContextID;

    await waitForRotated(TEST_CONTEXT_ID, async () => {
      let persisted = waitForPersist(instance);
      generatedContextID = await instance.request();
      await persisted;
    });

    Assert.ok(
      UUID_REGEX.test(generatedContextID),
      "Should have gotten a UUID generated for the context ID."
    );

    let creationTimestamp = Services.prefs.getIntPref(
      CONTEXT_ID_TIMESTAMP_PREF
    );
    // We should have bumped the creation timestamp.
    Assert.greater(creationTimestamp, 1);

    // We should NOT be able to synchronously request the context ID in this
    // configuration.
    Assert.throws(() => {
      instance.requestSynchronously();
    }, /Cannot request context ID synchronously/);
  });
});

/**
 * Test that if we have a pre-existing context ID, we can force rotation even
 * if the expiry hasn't come up.
 */
add_task(async function test_force_rotation() {
  Services.prefs.setCharPref(CONTEXT_ID_PREF, TEST_CONTEXT_ID);
  Services.prefs.clearUserPref(CONTEXT_ID_TIMESTAMP_PREF);
  // Let's say there's a 30 day rotation window.
  const ROTATION_DAYS = 30;
  Services.prefs.setIntPref(CONTEXT_ID_ROTATION_DAYS_PREF, ROTATION_DAYS);

  await withTestingContextId(async instance => {
    Assert.equal(
      await instance.request(),
      TEST_CONTEXT_ID,
      "Should have gotten the stored context ID"
    );

    await waitForRotated(TEST_CONTEXT_ID, async () => {
      await instance.forceRotation();
    });

    let generatedContextID = await instance.request();

    Assert.notEqual(
      generatedContextID,
      TEST_CONTEXT_ID,
      "The context ID should have been regenerated."
    );
    Assert.ok(
      UUID_REGEX.test(generatedContextID),
      "Should have gotten a UUID generated for the context ID."
    );

    // We should NOT be able to synchronously request the context ID in this
    // configuration.
    Assert.throws(() => {
      instance.requestSynchronously();
    }, /Cannot request context ID synchronously/);
  });
});
