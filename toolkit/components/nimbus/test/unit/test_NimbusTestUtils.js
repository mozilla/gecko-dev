"use strict";

add_task(async function test_recipe_fake_validates() {
  const recipe = NimbusTestUtils.factories.recipe("foo");
  await NimbusTestUtils.validateExperiment(recipe);
});

add_task(async function test_enrollmentHelper() {
  let recipe = NimbusTestUtils.factories.recipe.withFeatureConfig("bar", {
    featureId: "aboutwelcome",
  });
  let manager = NimbusTestUtils.stubs.manager();

  Assert.deepEqual(
    recipe.featureIds,
    ["aboutwelcome"],
    "Helper sets correct featureIds"
  );

  await manager.store.init();
  await manager.onStartup();

  const doEnrollmentCleanup = await NimbusTestUtils.enroll(recipe, {
    manager,
  });

  Assert.ok(manager.store.getAllActiveExperiments().length === 1, "Enrolled");
  Assert.equal(
    manager.store.getAllActiveExperiments()[0].slug,
    recipe.slug,
    "Has expected slug"
  );
  Assert.ok(
    Services.prefs.prefHasUserValue("nimbus.syncdatastore.aboutwelcome"),
    "Sync pref cache set"
  );

  await doEnrollmentCleanup();

  Assert.ok(manager.store.getAll().length === 0, "Cleanup done");
  Assert.ok(
    !Services.prefs.prefHasUserValue("nimbus.syncdatastore.aboutwelcome"),
    "Sync pref cache is cleared"
  );
});

add_task(async function test_enrollWithFeatureConfig() {
  Services.prefs.setBoolPref("nimbus.telemetry.targetingContextEnabled", false);
  const { manager, cleanup } = await NimbusTestUtils.setupTest({
    features: [new ExperimentFeature("enrollWithFeatureConfig", {})],
  });

  let doEnrollmentCleanup = await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: "enrollWithFeatureConfig",
      value: { enabled: true },
    },
    { manager }
  );

  Assert.ok(
    manager.store.hasExperimentForFeature("enrollWithFeatureConfig"),
    "Enrolled successfully"
  );

  await doEnrollmentCleanup();

  Assert.ok(
    !manager.store.hasExperimentForFeature("enrollWithFeatureConfig"),
    "Unenrolled successfully"
  );

  await cleanup();
  Services.prefs.clearUserPref("nimbus.telemetry.targetingContextEnabled");
});
