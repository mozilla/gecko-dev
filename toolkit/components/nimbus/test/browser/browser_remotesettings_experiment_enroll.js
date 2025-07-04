"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["messaging-system.log", "all"],
      ["datareporting.healthreport.uploadEnabled", true],
      ["app.shield.optoutstudies.enabled", true],
    ],
  });

  await ExperimentAPI.ready();
  await ExperimentAPI._rsLoader.finishedUpdating();

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
});

add_task(async function test_experimentEnrollment() {
  // Need to randomize the slug so subsequent test runs don't skip enrollment
  // due to a conflicting slug
  const recipe = NimbusTestUtils.factories.recipe("foo" + Math.random());
  await resetRemoteSettingsCollections({ experiments: [recipe] });

  await ExperimentAPI._rsLoader.updateRecipes("mochitest");

  let meta = NimbusFeatures.testFeature.getEnrollmentMetadata();
  Assert.equal(meta.slug, recipe.slug, "Enrollment active");

  ExperimentAPI.manager.unenroll(recipe.slug);

  meta = NimbusFeatures.testFeature.getEnrollmentMetadata();
  Assert.ok(!meta, "Experiment is no longer active");

  await NimbusTestUtils.removeStore(ExperimentAPI.manager.store);

  await resetRemoteSettingsCollections();
});

add_task(async function test_experimentEnrollment_startup() {
  // Studies pref can turn the feature off but if the feature pref is off
  // then it stays off.
  await SpecialPowers.pushPrefEnv({
    set: [["app.shield.optoutstudies.enabled", false]],
  });

  Assert.ok(!ExperimentAPI._rsLoader._enabled, "Should be disabled");

  await SpecialPowers.pushPrefEnv({
    set: [["app.shield.optoutstudies.enabled", true]],
  });

  Assert.ok(ExperimentAPI._rsLoader._enabled, "Should be enabled");
});
