"use strict";

const FEATURE_ID = "aboutwelcome";
const TEST_FALLBACK_PREF = "browser.aboutwelcome.screens";
const FAKE_FEATURE_MANIFEST = {
  variables: {
    screens: {
      type: "json",
      fallbackPref: TEST_FALLBACK_PREF,
    },
    source: {
      type: "string",
    },
  },
};

add_task(
  async function test_ExperimentFeature_getAllVariables_prefsOverDefaults() {
    const { cleanup } = await NimbusTestUtils.setupTest();

    const featureInstance = new ExperimentFeature(
      FEATURE_ID,
      FAKE_FEATURE_MANIFEST
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

    Assert.equal(
      featureInstance.getAllVariables().screens?.length,
      undefined,
      "pref is not set"
    );

    Services.prefs.setStringPref(TEST_FALLBACK_PREF, "[]");

    Assert.deepEqual(
      featureInstance.getAllVariables().screens.length,
      0,
      "should return the user pref value over the defaults"
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

    await cleanup();
  }
);

add_task(
  async function test_ExperimentFeature_getAllVariables_experimentOverPref() {
    const { manager, cleanup } = await NimbusTestUtils.setupTest();
    const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
      "awexperiment",
      {
        branchSlug: "treatment",
        featureId: "aboutwelcome",
        value: { screens: ["test-value"] },
      }
    );

    await manager.enroll(recipe, "test");

    const featureInstance = new ExperimentFeature(
      FEATURE_ID,
      FAKE_FEATURE_MANIFEST
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

    Assert.ok(
      !!featureInstance.getAllVariables().screens,
      "should return the AW experiment value"
    );

    Assert.equal(
      featureInstance.getAllVariables().screens[0],
      "test-value",
      "should return the AW experiment value"
    );

    Services.prefs.setStringPref(TEST_FALLBACK_PREF, "[]");
    Assert.equal(
      featureInstance.getAllVariables().screens[0],
      "test-value",
      "should return the AW experiment value"
    );

    await NimbusTestUtils.cleanupManager([recipe.slug], { manager });
    Assert.deepEqual(
      featureInstance.getAllVariables().screens.length,
      0,
      "should return the user pref value"
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

    await cleanup();
  }
);

add_task(
  async function test_ExperimentFeature_getAllVariables_experimentOverRemote() {
    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);
    const { manager, cleanup } = await NimbusTestUtils.setupTest();
    const featureInstance = new ExperimentFeature(
      FEATURE_ID,
      FAKE_FEATURE_MANIFEST
    );

    const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
      "aw-experiment",
      {
        branchSlug: "treatment",
        featureId: FEATURE_ID,
        value: { screens: ["test-value"] },
      }
    );
    const rollout = NimbusTestUtils.factories.recipe.withFeatureConfig(
      "aw-rollout",
      {
        branchSlug: "treatment",
        featureId: FEATURE_ID,
        value: { screens: [], source: "rollout" },
      },
      { isRollout: true }
    );
    // We're using the store in this test we need to wait for it to load
    await manager.store.ready();

    await manager.enroll(recipe, "test");
    await manager.enroll(rollout, "test");

    const allVariables = featureInstance.getAllVariables();

    Assert.equal(allVariables.screens.length, 1, "Returns experiment value");
    Assert.ok(!allVariables.source, "Does not include rollout value");

    await NimbusTestUtils.cleanupManager([recipe.slug, rollout.slug], {
      manager,
    });

    await cleanup();
  }
);

add_task(
  async function test_ExperimentFeature_getAllVariables_rolloutOverPrefDefaults() {
    const { manager, cleanup } = await NimbusTestUtils.setupTest();
    const featureInstance = new ExperimentFeature(
      FEATURE_ID,
      FAKE_FEATURE_MANIFEST
    );
    const rollout = NimbusTestUtils.factories.recipe.withFeatureConfig(
      "foo-aw",
      {
        branchSlug: "getAllVariables",
        featureId: FEATURE_ID,
        value: { screens: [] },
      },
      { isRollout: true }
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

    Assert.equal(
      featureInstance.getAllVariables().screens?.length,
      undefined,
      "Pref is not set"
    );

    await manager.enroll(rollout, "test");

    Assert.deepEqual(
      featureInstance.getAllVariables().screens?.length,
      0,
      "Should return the rollout value over the defaults"
    );

    Services.prefs.setStringPref(TEST_FALLBACK_PREF, "[1,2,3]");

    Assert.deepEqual(
      featureInstance.getAllVariables().screens.length,
      0,
      "should return the rollout value over the user pref"
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

    manager.unenroll(rollout.slug);
    await cleanup();
  }
);

add_task(
  async function test_ExperimentFeature_getAllVariables_defaultValuesParam() {
    const { cleanup } = await NimbusTestUtils.setupTest();
    const featureInstance = new ExperimentFeature(
      FEATURE_ID,
      FAKE_FEATURE_MANIFEST
    );

    Services.prefs.clearUserPref(TEST_FALLBACK_PREF);

    Assert.equal(
      featureInstance.getAllVariables({ defaultValues: { screens: null } })
        .screens,
      null,
      "should return defaultValues param over default pref settings"
    );

    await cleanup();
  }
);

add_task(async function testGetAllVariablesCoenrolling() {
  const cleanupFeature = NimbusTestUtils.addTestFeatures(
    new ExperimentFeature("foo", {
      allowCoenrollment: true,
      variables: {
        bar: {
          type: "string",
        },
      },
    })
  );

  Assert.throws(
    () => NimbusFeatures.foo.getAllVariables(),
    /Co-enrolling features must use the getAllEnrollments API/
  );

  cleanupFeature();
});
