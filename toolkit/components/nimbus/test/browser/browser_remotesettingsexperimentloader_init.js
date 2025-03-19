/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { NimbusTelemetry } = ChromeUtils.importESModule(
  "resource://nimbus/lib/Telemetry.sys.mjs"
);

function getRecipe(slug) {
  return ExperimentFakes.recipe(slug, {
    bucketConfig: {
      start: 0,
      // Make sure the experiment enrolls
      count: 10000,
      total: 10000,
      namespace: "mochitest",
      randomizationUnit: "normandy_id",
    },
  });
}

add_task(async function test_double_feature_enrollment() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(NimbusTelemetry, "recordEnrollmentFailure");
  await ExperimentAPI.ready();

  Assert.ok(
    ExperimentManager.store.getAllActiveExperiments().length === 0,
    "Clean state"
  );

  let recipe1 = getRecipe("foo" + Math.random());
  let recipe2 = getRecipe("bar" + Math.random());

  await ExperimentManager.enroll(recipe1, "test_double_feature_enrollment");
  await ExperimentManager.enroll(recipe2, "test_double_feature_enrollment");

  Assert.equal(
    ExperimentManager.store.getAllActiveExperiments().length,
    1,
    "1 active experiment"
  );

  Assert.equal(
    NimbusTelemetry.recordEnrollmentFailure.callCount,
    1,
    "Expected to fail one of the recipes"
  );

  Assert.ok(
    NimbusTelemetry.recordEnrollmentFailure.calledOnceWith(
      recipe2.slug,
      "feature-conflict"
    )
  );

  await ExperimentFakes.cleanupAll([recipe1.slug]);
  sandbox.restore();
});
