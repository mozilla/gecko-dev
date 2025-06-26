/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { NimbusTelemetry } = ChromeUtils.importESModule(
  "resource://nimbus/lib/Telemetry.sys.mjs"
);

add_task(async function test_double_feature_enrollment() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(NimbusTelemetry, "recordEnrollmentFailure");
  await ExperimentAPI.ready();

  Assert.strictEqual(
    ExperimentAPI.manager.store.getAllActiveExperiments().length,
    0,
    "Clean state"
  );

  let recipe1 = NimbusTestUtils.factories.recipe("foo" + Math.random());
  let recipe2 = NimbusTestUtils.factories.recipe("bar" + Math.random());

  await ExperimentAPI.manager.enroll(recipe1, "test_double_feature_enrollment");
  await ExperimentAPI.manager.enroll(recipe2, "test_double_feature_enrollment");

  Assert.equal(
    ExperimentAPI.manager.store.getAllActiveExperiments().length,
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

  await NimbusTestUtils.cleanupManager([recipe1.slug]);
  sandbox.restore();
});
