/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

add_setup(() => {
  Services.fog.initializeFOG();
});

add_task(async function test_SUBMIT_ONBOARDING_OPT_OUT_PING() {
  // Arrange fake experiment enrollment details.
  const manager = ExperimentFakes.manager();
  sinon.stub(SpecialMessageActions, "_experimentManager").get(() => manager);

  await manager.onStartup();
  await manager.store.addEnrollment(ExperimentFakes.experiment("foo"));
  manager.unenroll("foo", "some-reason");
  await manager.store.addEnrollment(
    ExperimentFakes.experiment("bar", { active: false })
  );
  await manager.store.addEnrollment(
    ExperimentFakes.experiment("baz", { active: true })
  );

  manager.store.addEnrollment(ExperimentFakes.rollout("rol1"));
  manager.unenroll("rol1", "some-reason");
  manager.store.addEnrollment(ExperimentFakes.rollout("rol2"));

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
      { experimentSlug: "foo", branchSlug: "treatment" },
      { experimentSlug: "bar", branchSlug: "treatment" },
      { experimentSlug: "baz", branchSlug: "treatment" },
      { experimentSlug: "rol1", branchSlug: "treatment" },
      { experimentSlug: "rol2", branchSlug: "treatment" },
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
});
