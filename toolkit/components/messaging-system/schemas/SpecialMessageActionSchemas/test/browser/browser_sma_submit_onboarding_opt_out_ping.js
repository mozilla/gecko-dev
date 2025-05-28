/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { NimbusTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

NimbusTestUtils.init(this);

add_setup(async () => {
  Services.fog.initializeFOG();

  await ExperimentAPI.ready();
});

add_task(async function test_SUBMIT_ONBOARDING_OPT_OUT_PING() {
  // Arrange fake experiment enrollment details.
  await ExperimentAPI.manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
      featureId: "testFeature",
    }),
    "test"
  );
  await ExperimentAPI.manager.unenroll("foo");
  await ExperimentAPI.manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("bar", {
      featureId: "testFeature",
    }),
    "test"
  );
  await ExperimentAPI.manager.unenroll("bar");
  await ExperimentAPI.manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("baz", {
      featureId: "testFeature",
    }),
    "test"
  );

  await ExperimentAPI.manager.enroll(
    NimbusTestUtils.factories.recipe("rol1", { isRollout: true }),
    "test"
  );
  await ExperimentAPI.manager.unenroll("rol1");
  await ExperimentAPI.manager.enroll(
    NimbusTestUtils.factories.recipe("rol2", { isRollout: true }),
    "test"
  );

  let { promise, resolve } = Promise.withResolvers();

  GleanPings.onboardingOptOut.testBeforeNextSubmit(() => {
    // After SMA, everything is set.
    Assert.deepEqual(Glean.onboardingOptOut.activeExperiments.testGetValue(), [
      "baz",
    ]);
    Assert.deepEqual(Glean.onboardingOptOut.activeRollouts.testGetValue(), [
      "rol2",
    ]);

    // Map entry order is stable: we need not sort.
    let expected = [
      { experimentSlug: "foo", branchSlug: "control" },
      { experimentSlug: "bar", branchSlug: "control" },
      { experimentSlug: "baz", branchSlug: "control" },
      { experimentSlug: "rol1", branchSlug: "control" },
      { experimentSlug: "rol2", branchSlug: "control" },
    ];

    Assert.deepEqual(
      Glean.onboardingOptOut.enrollmentsMap.testGetValue(),
      expected
    );

    resolve(true);
  });

  // Before SMA, nothing is set.
  Assert.deepEqual(
    Glean.onboardingOptOut.activeExperiments.testGetValue(),
    null
  );
  Assert.deepEqual(Glean.onboardingOptOut.activeRollouts.testGetValue(), null);
  Assert.deepEqual(Glean.onboardingOptOut.enrollmentsMap.testGetValue(), null);

  await SMATestUtils.executeAndValidateAction({
    type: "SUBMIT_ONBOARDING_OPT_OUT_PING",
  });

  ok(await promise, "`onboarding-opt-out` ping was submitted");

  await NimbusTestUtils.cleanupManager(["baz", "rol2"]);
});
