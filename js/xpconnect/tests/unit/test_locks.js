/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that privileged scopes can access Web Locks and use them without
 * being associated with a content global.
 */
add_task(async function test_locks() {
  Assert.ok(locks, "The locks global was imported.");
  let { promise: firstPromise, resolve: firstResolve } =
    Promise.withResolvers();

  const LOCK_NAME = "Some lock";

  await locks.request(LOCK_NAME, async lock => {
    Assert.ok(lock, "Got the lock");

    let { held: heldLocks } = await locks.query();
    Assert.equal(heldLocks.length, 1, "Should only be 1 held lock");
    Assert.equal(heldLocks[0].name, LOCK_NAME, "Got the right lock name");
    Assert.equal(heldLocks[0].clientId, "", "Got an empty client ID");

    firstResolve();
  });
  await firstPromise;
});
