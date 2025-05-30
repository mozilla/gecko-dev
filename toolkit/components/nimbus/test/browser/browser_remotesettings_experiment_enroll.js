"use strict";

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

let rsClient;
let secureRsClient;

add_setup(async function () {
  rsClient = RemoteSettings("nimbus-desktop-experiments");
  secureRsClient = RemoteSettings("nimbus-secure-experiments");
  await rsClient.db.importChanges({}, Date.now(), [], { clear: true });
  await secureRsClient.db.importChanges({}, Date.now(), [], { clear: true });

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
    await rsClient.db.clear();
    await secureRsClient.db.clear();
  });
});

add_task(async function test_experimentEnrollment() {
  // Need to randomize the slug so subsequent test runs don't skip enrollment
  // due to a conflicting slug
  const recipe = NimbusTestUtils.factories.recipe("foo" + Math.random());
  await rsClient.db.importChanges({}, Date.now(), [recipe], {
    clear: true,
  });

  await ExperimentAPI._rsLoader.updateRecipes("mochitest");

  let meta = NimbusFeatures.testFeature.getEnrollmentMetadata();
  Assert.equal(meta.slug, recipe.slug, "Enrollment active");

  await ExperimentAPI.manager.unenroll(recipe.slug);

  meta = NimbusFeatures.testFeature.getEnrollmentMetadata();
  Assert.ok(!meta, "Experiment is no longer active");

  await NimbusTestUtils.removeStore(ExperimentAPI.manager.store);
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
