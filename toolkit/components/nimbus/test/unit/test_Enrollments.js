/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// todo: add tests

add_task(async function testEnrollments() {
  const { sandbox, manager, cleanup } = await NimbusTestUtils.setupTest();
  const db = manager.store._db;
  sandbox.spy(db, "updateEnrollment");

  Assert.equal(db.pendingWrites, 0, "No pending writes");

  const recipe = NimbusTestUtils.factories.recipe("test");
  await manager.enroll(recipe, "test");
  const enrollment = manager.store.get(recipe.slug);

  Assert.equal(db.pendingWrites, 1, "1 pending write");
  Assert.ok(db.updateEnrollment.calledOnceWithExactly(recipe.slug, recipe));

  Assert.equal(
    await NimbusTestUtils.queryEnrollment(recipe.slug),
    null,
    "Enrollment does not exist in database yet"
  );

  await NimbusTestUtils.flushStore();

  Assert.equal(db.pendingWrites, 0, "No remaining pending writes");

  {
    const dbEnrollment = await NimbusTestUtils.queryEnrollment(recipe.slug);
    Assert.notEqual(dbEnrollment, null, "Enrollment exists in database");

    Assert.equal(
      dbEnrollment.branchSlug,
      enrollment.branch.slug,
      "Has the correct branch slug"
    );
    Assert.deepEqual(dbEnrollment.recipe, recipe, "Has the recipe attached");
  }

  db.updateEnrollment.resetHistory();

  manager.unenroll("test", { reason: "reason123" });

  Assert.equal(db.pendingWrites, 1, "1 pending write");
  Assert.ok(db.updateEnrollment.calledOnceWithExactly(recipe.slug));

  {
    const dbEnrollment = await NimbusTestUtils.queryEnrollment(recipe.slug);
    Assert.notEqual(
      dbEnrollment,
      null,
      "Enrollment still exists in the database"
    );
    Assert.ok(dbEnrollment.active, "database change has not been flushed yet");
    Assert.equal(dbEnrollment.unenrollReason, null, "Unenroll reason");
  }

  await NimbusTestUtils.flushStore();

  {
    const dbEnrollment = await NimbusTestUtils.queryEnrollment(recipe.slug);
    Assert.notEqual(
      dbEnrollment,
      null,
      "Enrollment still exists in the database"
    );
    Assert.ok(!dbEnrollment.active, "Enrollment no longer active");
    Assert.equal(dbEnrollment.unenrollReason, "reason123", "Unenroll reason");
    Assert.equal(dbEnrollment.recipe, null, "Recipe is now null");
  }

  manager.store._removeEntriesByKeys([recipe.slug]);

  // This will trigger a DELETE query.
  db.updateEnrollment(recipe.slug);

  Assert.equal(db.pendingWrites, 1, "1 pending write");

  Assert.notEqual(
    await NimbusTestUtils.queryEnrollment(recipe.slug),
    null,
    "Enrollment still exists"
  );

  await NimbusTestUtils.flushStore();

  Assert.equal(
    await NimbusTestUtils.queryEnrollment(recipe.slug),
    null,
    "Enrollment deleted from database"
  );

  await cleanup();
});

add_task(async function testCollapseWrites() {
  const { manager, cleanup } = await NimbusTestUtils.setupTest();
  const db = manager.store._db;

  const recipe = NimbusTestUtils.factories.recipe("unenroll-immediately");

  await manager.enroll(recipe, "test");
  manager.unenroll(recipe.slug, { reason: "some reason" });

  Assert.equal(db.pendingWrites, 1, "1 pending write");

  await NimbusTestUtils.flushStore();

  Assert.equal(db.pendingWrites, 0, "No pending writes");

  const dbEnrollment = await NimbusTestUtils.queryEnrollment(recipe.slug);

  Assert.notEqual(dbEnrollment, null, "Enrollment exists in database");
  Assert.ok(!dbEnrollment.active, "Enrollment is not active");
  Assert.equal(dbEnrollment.unenrollReason, "some reason");
  Assert.equal(dbEnrollment.recipe, null, "Recipe is null");

  await cleanup();
});
