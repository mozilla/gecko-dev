"use strict";

const { Sampling } = ChromeUtils.importESModule(
  "resource://gre/modules/components-utils/Sampling.sys.mjs"
);

const { ClientID } = ChromeUtils.importESModule(
  "resource://gre/modules/ClientID.sys.mjs"
);
const { ClientEnvironment } = ChromeUtils.importESModule(
  "resource://normandy/lib/ClientEnvironment.sys.mjs"
);
const { ExperimentStore } = ChromeUtils.importESModule(
  "resource://nimbus/lib/ExperimentStore.sys.mjs"
);
const { NimbusTelemetry } = ChromeUtils.importESModule(
  "resource://nimbus/lib/Telemetry.sys.mjs"
);
const { TelemetryEnvironment } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryEnvironment.sys.mjs"
);

const { SYNC_DATA_PREF_BRANCH, SYNC_DEFAULTS_PREF_BRANCH } = ExperimentStore;

add_setup(function test_setup() {
  Services.fog.initializeFOG();

  registerCleanupFunction(
    NimbusTestUtils.addTestFeatures(
      new ExperimentFeature("optin", {}),
      new ExperimentFeature("pink", {}),
      new ExperimentFeature("force-enrollment", {})
    )
  );
});

function setupTest({ ...args } = {}) {
  return NimbusTestUtils.setupTest({ ...args, clearTelemetry: true });
}

/**
 * The normal case: Enrollment of a new experiment
 */
add_task(async function test_add_to_store() {
  const { manager, cleanup } = await setupTest();

  const recipe = NimbusTestUtils.factories.recipe("foo");
  await manager.enroll(recipe, "test_add_to_store");
  const experiment = manager.store.get("foo");

  Assert.ok(experiment, "should add an experiment with slug foo");
  Assert.ok(
    recipe.branches.includes(experiment.branch),
    "should choose a branch from the recipe.branches"
  );
  Assert.equal(experiment.active, true, "should set .active = true");

  await manager.unenroll("foo");

  await cleanup();
});

add_task(async function test_add_rollout_to_store() {
  const { manager, cleanup } = await NimbusTestUtils.setupTest();

  const recipe = {
    ...NimbusTestUtils.factories.recipe("rollout-slug"),
    branches: [NimbusTestUtils.factories.rollout("rollout").branch],
    isRollout: true,
    active: true,
    bucketConfig: {
      namespace: "nimbus-test-utils",
      randomizationUnit: "normandy_id",
      start: 0,
      count: 1000,
      total: 1000,
    },
  };

  await manager.enroll(recipe, "test_add_rollout_to_store");
  const experiment = manager.store.get("rollout-slug");

  Assert.ok(experiment, `Should add an experiment with slug ${recipe.slug}`);
  Assert.ok(
    recipe.branches.includes(experiment.branch),
    "should choose a branch from the recipe.branches"
  );
  Assert.equal(experiment.isRollout, true, "should have .isRollout");

  await manager.unenroll("rollout-slug");

  await cleanup();
});

add_task(async function test_enroll_optin_recipe_branch_selection() {
  const { sandbox, manager, cleanup } = await setupTest();

  // stubbing this to return true since we don't want to actually enroll
  // just assert on the call
  sandbox.stub(manager, "_enroll").returns(true);

  await manager.store.init();
  await manager.onStartup();

  const optInRecipe = NimbusTestUtils.factories.recipe("opt-in-recipe", {
    isFirefoxLabsOptIn: true,
    branches: [
      {
        slug: "opt-in-recipe-branch-slug",
        ratio: 1,
        features: [{ featureId: "optin", value: {} }],
      },
    ],
  });

  // Call with missing optInRecipeBranchSlug argument
  await Assert.rejects(
    manager.enroll(optInRecipe, "test"),
    /Branch slug not provided for Firefox Labs opt in recipe: "opt-in-recipe"/,
    "Should not enroll an opt-in recipe with missing optInBranchSlug"
  );

  // Call with incorrect optInRecipeBranchSlug for the optin recipe
  await Assert.rejects(
    manager.enroll(optInRecipe, "test", { branchSlug: "invalid-slug" }),
    /Invalid branch slug provided for Firefox Labs opt in recipe: "opt-in-recipe"/,
    "Should not enroll an opt-in recipe with invalid branch slug"
  );

  // Call with the correct branch slug
  await manager.enroll(optInRecipe, "test", {
    branchSlug: optInRecipe.branches[0].slug,
  });

  Assert.ok(
    manager._enroll.calledOnceWith(
      optInRecipe,
      optInRecipe.branches[0].slug,
      "test"
    ),
    "should call ._enroll() with the correct arguments"
  );

  await cleanup();
});

add_task(async function test_setExperimentActive_recordEnrollment_called() {
  const { sandbox, manager, cleanup } = await setupTest();

  sandbox.spy(NimbusTelemetry, "setExperimentActive");
  sandbox.spy(NimbusTelemetry, "recordEnrollment");

  await manager.store.init();
  await manager.onStartup();

  // Ensure there is no experiment active with the id in FOG
  Assert.equal(
    undefined,
    Services.fog.testGetExperimentData("foo"),
    "no active experiment exists before enrollment"
  );

  // Check that there aren't any Glean enrollment events yet
  var enrollmentEvents = Glean.nimbusEvents.enrollment.testGetValue("events");
  Assert.equal(
    undefined,
    enrollmentEvents,
    "no Glean enrollment events before enrollment"
  );

  await manager.enroll(
    NimbusTestUtils.factories.recipe("foo"),
    "test_setExperimentActive_sendEnrollmentTelemetry_called"
  );
  const experiment = manager.store.get("foo");

  Assert.equal(
    NimbusTelemetry.setExperimentActive.calledWith(experiment),
    true,
    "should call setExperimentActive after an enrollment"
  );

  Assert.equal(
    NimbusTelemetry.recordEnrollment.calledWith(experiment),
    true,
    "should call recordEnrollment after an enrollment"
  );

  // Test Glean experiment API interaction
  Assert.notEqual(
    undefined,
    Services.fog.testGetExperimentData(experiment.slug),
    "Glean.setExperimentActive called with `foo` feature"
  );

  // Check that the Glean enrollment event was recorded.
  enrollmentEvents = Glean.nimbusEvents.enrollment.testGetValue("events");
  // We expect only one event
  Assert.equal(1, enrollmentEvents.length);
  // And that one event matches the expected enrolled experiment
  Assert.equal(
    experiment.slug,
    enrollmentEvents[0].extra.experiment,
    "Glean.nimbusEvents.enrollment recorded with correct experiment slug"
  );
  Assert.equal(
    experiment.branch.slug,
    enrollmentEvents[0].extra.branch,
    "Glean.nimbusEvents.enrollment recorded with correct branch slug"
  );

  await manager.unenroll("foo");

  await cleanup();
});

add_task(async function test_setRolloutActive_recordEnrollment_called() {
  const { sandbox, manager, cleanup } = await setupTest();

  const rolloutRecipe = NimbusTestUtils.factories.recipe("rollout", {
    isRollout: true,
  });
  sandbox.spy(TelemetryEnvironment, "setExperimentActive");
  sandbox.spy(NimbusTelemetry, "setExperimentActive");
  sandbox.spy(NimbusTelemetry, "recordEnrollment");

  await manager.store.init();
  await manager.onStartup();

  // Test Glean experiment API interaction
  Assert.equal(
    undefined,
    Services.fog.testGetExperimentData("rollout"),
    "no rollout active before enrollment"
  );

  // Check that there aren't any Glean enrollment events yet
  Assert.equal(
    Glean.nimbusEvents.enrollment.testGetValue("events"),
    undefined,
    "no Glean enrollment events before enrollment"
  );

  // Check that there aren't any Glean normandy enrollNimbusExperiment events yet
  Assert.equal(
    Glean.normandy.enrollNimbusExperiment.testGetValue("events"),
    undefined,
    "no Glean normandy enrollment events before enrollment"
  );

  let result = await manager.enroll(rolloutRecipe, "test");

  const enrollment = manager.store.get("rollout");

  Assert.ok(!!result && !!enrollment, "Enrollment was successful");

  Assert.ok(
    TelemetryEnvironment.setExperimentActive.called,
    "should call setExperimentActive"
  );
  Assert.ok(
    NimbusTelemetry.setExperimentActive.calledWith(enrollment),
    "Should call setExperimentActive with the rollout"
  );
  Assert.equal(
    NimbusTelemetry.recordEnrollment.calledWith(enrollment),
    true,
    "should call sendEnrollmentTelemetry after an enrollment"
  );

  // We expect only one event and that that one event matches the expected enrolled experiment
  Assert.deepEqual(
    Glean.normandy.enrollNimbusExperiment
      .testGetValue("events")
      .map(ev => ev.extra),
    [
      {
        value: enrollment.slug,
        branch: enrollment.branch.slug,
        experimentType: "rollout",
      },
    ]
  );

  // Test Glean experiment API interaction
  Assert.equal(
    enrollment.branch.slug,
    Services.fog.testGetExperimentData(enrollment.slug).branch,
    "Glean.setExperimentActive called with expected values"
  );

  // We expect only one event and that that one event matches the expected enrolled experiment
  Assert.deepEqual(
    Glean.nimbusEvents.enrollment.testGetValue("events").map(ev => ev.extra),
    [
      {
        experiment: enrollment.slug,
        branch: enrollment.branch.slug,
        experiment_type: "rollout",
      },
    ]
  );

  await manager.unenroll("rollout");

  await cleanup();
});

// /**
//  * Failure cases:
//  * - slug conflict
//  * - group conflict
//  */

add_task(async function test_failure_name_conflict() {
  const { sandbox, manager, cleanup } = await setupTest();

  sandbox.spy(NimbusTelemetry, "recordEnrollmentFailure");

  // Check that there aren't any Glean enroll_failed events yet
  Assert.equal(
    Glean.nimbusEvents.enrollFailed.testGetValue("events"),
    null,
    "no Glean enroll_failed events before failure"
  );

  const experiment = NimbusTestUtils.factories.recipe.withFeatureConfig("foo", {
    featureId: "testFeature",
  });

  // simulate adding a previouly enrolled experiment
  await manager.enroll(experiment, "test");

  await Assert.rejects(
    manager.enroll(experiment, "test_failure_name_conflict"),
    /An experiment with the slug "foo" already exists/,
    "should throw if a conflicting experiment exists"
  );

  // Check that the Glean events were recorded.
  Assert.deepEqual(
    Glean.nimbusEvents.enrollFailed.testGetValue("events").map(ev => ev.extra),
    [
      {
        experiment: "foo",
        reason: "name-conflict",
      },
    ],
    "enrollFailed telemetry recorded correctly"
  );

  Assert.deepEqual(
    Glean.nimbusEvents.enrollmentStatus
      .testGetValue("events")
      .map(ev => ev.extra),
    [
      {
        slug: "foo",
        status: "Enrolled",
        reason: "Qualified",
        branch: "control",
      },
      {
        slug: "foo",
        status: "NotEnrolled",
        reason: "NameConflict",
      },
    ],
    "enrollmentStatus telemetry recorded correctly"
  );

  await manager.unenroll("foo");

  await cleanup();
});

add_task(async function test_failure_group_conflict() {
  const { sandbox, manager, cleanup } = await setupTest();

  sandbox.spy(NimbusTelemetry, "recordEnrollmentFailure");

  // Check that there aren't any Glean enroll_failed events yet
  var failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue("events");
  Assert.equal(
    undefined,
    failureEvents,
    "no Glean enroll_failed events before failure"
  );

  // Two conflicting branches that both have the group "pink"
  // These should not be allowed to exist simultaneously.
  const existingBranch = {
    slug: "treatment",
    ratio: 1,
    features: [{ featureId: "pink", value: {} }],
  };
  const newBranch = {
    slug: "treatment",
    ratio: 1,
    features: [{ featureId: "pink", value: {} }],
  };

  // simulate adding an experiment with a conflicting group "pink"
  await manager.enroll(
    NimbusTestUtils.factories.recipe("foo", {
      branches: [existingBranch],
    }),
    "test_failure_group_conflict"
  );

  Assert.equal(
    await manager.enroll(
      NimbusTestUtils.factories.recipe("bar", { branches: [newBranch] }),
      "test_failure_group_conflict"
    ),
    null,
    "should not enroll if there is a feature conflict"
  );

  Assert.equal(
    NimbusTelemetry.recordEnrollmentFailure.calledWith(
      "bar",
      "feature-conflict"
    ),
    true,
    "should send failure telemetry if a feature conflict exists"
  );

  // Check that the Glean enroll_failed event was recorded.
  failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue("events");
  // We expect only one event
  Assert.equal(1, failureEvents.length);
  // And that event matches the expected experiment and reason
  Assert.equal(
    "bar",
    failureEvents[0].extra.experiment,
    "Glean.nimbusEvents.enroll_failed recorded with correct experiment slug"
  );
  Assert.equal(
    "feature-conflict",
    failureEvents[0].extra.reason,
    "Glean.nimbusEvents.enroll_failed recorded with correct reason"
  );

  await manager.unenroll("foo");

  await cleanup();
});

add_task(async function test_rollout_failure_group_conflict() {
  const { sandbox, manager, cleanup } = await setupTest();

  sandbox.spy(NimbusTelemetry, "recordEnrollmentFailure");

  const recipe = NimbusTestUtils.factories.recipe("rollout-recipe", {
    isRollout: true,
  });
  const conflictingRecipe = {
    ...recipe,
    slug: "conflicting-rollout-recipe",
  };

  // Check that there aren't any Glean enroll_failed events yet
  var failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue("events");
  Assert.equal(
    undefined,
    failureEvents,
    "no Glean enroll_failed events before failure"
  );

  await manager.enroll(recipe, "test_rollout_failure_group_conflict");

  Assert.equal(
    await manager.enroll(
      conflictingRecipe,
      "test_rollout_failure_group_conflict"
    ),
    null,
    "should not enroll if there is a feature conflict"
  );

  Assert.ok(
    NimbusTelemetry.recordEnrollmentFailure.calledWith(
      conflictingRecipe.slug,
      "feature-conflict"
    ),
    "should send failure telemetry if a feature conflict exists"
  );

  // Check that the Glean enroll_failed event was recorded.
  failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue("events");
  // We expect only one event
  Assert.equal(1, failureEvents.length);
  // And that event matches the expected experiment and reason
  Assert.equal(
    conflictingRecipe.slug,
    failureEvents[0].extra.experiment,
    "Glean.nimbusEvents.enroll_failed recorded with correct experiment slug"
  );
  Assert.equal(
    "feature-conflict",
    failureEvents[0].extra.reason,
    "Glean.nimbusEvents.enroll_failed recorded with correct reason"
  );

  await manager.unenroll("rollout-recipe");

  await cleanup();
});

add_task(async function test_rollout_experiment_no_conflict() {
  const { sandbox, manager, cleanup } = await setupTest();

  sandbox.spy(NimbusTelemetry, "recordEnrollmentFailure");

  const experiment = NimbusTestUtils.factories.recipe("experiment");
  const rollout = NimbusTestUtils.factories.recipe("rollout", {
    isRollout: true,
  });

  // Check that there aren't any Glean enroll_failed events yet
  var failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue("events");
  Assert.equal(
    undefined,
    failureEvents,
    "no Glean enroll_failed events before failure"
  );

  await NimbusTestUtils.enroll(experiment, {
    manager,
  });
  await NimbusTestUtils.enroll(rollout, {
    manager,
  });

  Assert.ok(
    manager.store.get(experiment.slug).active,
    "Enrolled in the experiment for the feature"
  );

  Assert.ok(
    manager.store.get(rollout.slug).active,
    "Enrolled in the rollout for the feature"
  );

  Assert.ok(
    NimbusTelemetry.recordEnrollmentFailure.notCalled,
    "Should send failure telemetry if a feature conflict exists"
  );

  // Check that there aren't any Glean enroll_failed events
  failureEvents = Glean.nimbusEvents.enrollFailed.testGetValue("events");
  Assert.equal(
    undefined,
    failureEvents,
    "no Glean enroll_failed events before failure"
  );

  await NimbusTestUtils.cleanupManager([experiment.slug, rollout.slug], {
    manager,
  });

  await cleanup();
});

add_task(async function test_sampling_check() {
  const { sandbox, manager, cleanup } = await setupTest();

  sandbox.stub(Sampling, "bucketSample").resolves(true);
  sandbox.replaceGetter(ClientEnvironment, "userId", () => 42);

  let recipe = NimbusTestUtils.factories.recipe("foo", { bucketConfig: null });

  Assert.ok(
    !(await manager.isInBucketAllocation(recipe.bucketConfig)),
    "fails for no bucket config"
  );

  recipe = NimbusTestUtils.factories.recipe("foo2", {
    bucketConfig: { randomizationUnit: "foo" },
  });

  Assert.ok(
    !(await manager.isInBucketAllocation(recipe.bucketConfig)),
    "fails for unknown randomizationUnit"
  );

  recipe = NimbusTestUtils.factories.recipe("foo3");

  const result = await manager.isInBucketAllocation(recipe.bucketConfig);

  Assert.equal(
    Sampling.bucketSample.callCount,
    1,
    "it should call bucketSample"
  );
  Assert.ok(result, "result should be true");
  const { args } = Sampling.bucketSample.firstCall;
  Assert.equal(args[0][0], 42, "called with expected randomization id");
  Assert.equal(
    args[0][1],
    recipe.bucketConfig.namespace,
    "called with expected namespace"
  );
  Assert.equal(
    args[1],
    recipe.bucketConfig.start,
    "called with expected start"
  );
  Assert.equal(
    args[2],
    recipe.bucketConfig.count,
    "called with expected count"
  );
  Assert.equal(
    args[3],
    recipe.bucketConfig.total,
    "called with expected total"
  );

  await cleanup();
});

add_task(async function enroll_in_reference_aw_experiment() {
  const { manager, cleanup } = await setupTest();

  let dir = Services.dirsvc.get("CurWorkD", Ci.nsIFile).path;
  let src = PathUtils.join(
    dir,
    "reference_aboutwelcome_experiment_content.json"
  );
  const content = await IOUtils.readJSON(src);
  // Create two dummy branches with the content from disk
  const branches = ["treatment-a", "treatment-b"].map(slug => ({
    slug,
    ratio: 1,
    features: [
      { value: { ...content, enabled: true }, featureId: "aboutwelcome" },
    ],
  }));
  let recipe = NimbusTestUtils.factories.recipe("reference-aw", { branches });
  // Ensure we get enrolled
  recipe.bucketConfig.count = recipe.bucketConfig.total;

  await manager.enroll(recipe, "enroll_in_reference_aw_experiment");

  Assert.ok(manager.store.get("reference-aw"), "Successful onboarding");
  let prefValue = Services.prefs.getStringPref(
    `${SYNC_DATA_PREF_BRANCH}aboutwelcome`
  );
  Assert.ok(
    prefValue,
    "aboutwelcome experiment enrollment should be stored to prefs"
  );
  // In case some regression causes us to store a significant amount of data
  // in prefs.
  Assert.ok(prefValue.length < 3498, "Make sure we don't bloat the prefs");

  await manager.unenroll(recipe.slug);

  await cleanup();
});

add_task(async function test_forceEnroll_cleanup() {
  const { sandbox, manager, cleanup } = await setupTest();

  sandbox.spy(manager, "_unenroll");

  const existingRecipe = NimbusTestUtils.factories.recipe("foo", {
    branches: [
      {
        slug: "treatment",
        ratio: 1,
        features: [{ featureId: "force-enrollment", value: {} }],
      },
    ],
  });
  const forcedRecipe = NimbusTestUtils.factories.recipe("bar", {
    branches: [
      {
        slug: "treatment",
        ratio: 1,
        features: [{ featureId: "force-enrollment", value: {} }],
      },
    ],
  });

  await manager.enroll(existingRecipe, "test_forceEnroll_cleanup");

  sandbox.spy(NimbusTelemetry, "setExperimentActive");
  await manager.forceEnroll(forcedRecipe, forcedRecipe.branches[0]);

  Assert.deepEqual(
    Glean.nimbusEvents.enrollmentStatus
      .testGetValue("events")
      ?.map(ev => ev.extra),
    [
      {
        slug: "foo",
        branch: "treatment",
        reason: "Qualified",
        status: "Enrolled",
      },
      {
        slug: "foo",
        branch: "treatment",
        status: "Disqualified",
        reason: "ForceEnrollment",
      },
      {
        slug: "optin-bar",
        branch: "treatment",
        status: "Enrolled",
        reason: "OptIn",
      },
    ]
  );

  Assert.ok(
    manager._unenroll.calledOnceWith(
      sinon.match({ slug: existingRecipe.slug }),
      { reason: "force-enrollment" }
    ),
    "Unenrolled from existing experiment"
  );
  Assert.ok(
    NimbusTelemetry.setExperimentActive.calledOnceWith(
      sinon.match({ slug: "optin-bar" })
    ),
    "Activated forced experiment"
  );
  Assert.ok(
    manager.store.get("optin-bar")?.active,
    "Enrolled in forced experiment"
  );

  await manager.unenroll(`optin-bar`);

  await cleanup();
});

add_task(async function test_rollout_unenroll_conflict() {
  const { sandbox, manager, cleanup } = await setupTest();

  sandbox.spy(manager, "_unenroll");

  const conflictingRollout = NimbusTestUtils.factories.recipe(
    "conflicting-rollout",
    { isRollout: true }
  );

  const rollout = NimbusTestUtils.factories.recipe("rollout", {
    isRollout: true,
  });

  // We want to force a conflict
  await manager.enroll(conflictingRollout, "rs-loader");

  await manager.forceEnroll(rollout, rollout.branches[0]);

  Assert.ok(
    manager._unenroll.calledOnceWith(
      sinon.match({ slug: conflictingRollout.slug }),
      { reason: "force-enrollment" }
    ),
    "Should unenroll the conflicting rollout"
  );

  Assert.ok(
    !manager.store.get(conflictingRollout.slug)?.active,
    "Conflicting rollout should be inactive"
  );
  Assert.ok(
    manager.store.get(`optin-${rollout.slug}`)?.active,
    "Rollout should be active"
  );

  await manager.unenroll(`optin-${rollout.slug}`);

  await cleanup();
});

add_task(async function test_forceEnroll() {
  const experiment1 = NimbusTestUtils.factories.recipe("experiment-1");
  const experiment2 = NimbusTestUtils.factories.recipe("experiment-2");
  const rollout1 = NimbusTestUtils.factories.recipe("rollout-1", {
    isRollout: true,
  });
  const rollout2 = NimbusTestUtils.factories.recipe("rollout-2", {
    isRollout: true,
  });

  const TEST_CASES = [
    {
      enroll: [experiment1, rollout1],
      expected: [experiment1, rollout1],
    },
    {
      enroll: [rollout1, experiment1],
      expected: [experiment1, rollout1],
    },
    {
      enroll: [experiment1, experiment2],
      expected: [experiment2],
    },
    {
      enroll: [rollout1, rollout2],
      expected: [rollout2],
    },
    {
      enroll: [experiment1, rollout1, rollout2, experiment2],
      expected: [experiment2, rollout2],
    },
  ];

  const { manager, cleanup } = await setupTest({
    experiments: [experiment1, experiment2, rollout1, rollout2],
  });

  for (const { enroll, expected } of TEST_CASES) {
    for (const recipe of enroll) {
      await manager.forceEnroll(recipe, recipe.branches[0]);
    }

    const activeSlugs = manager.store
      .getAll()
      .filter(enrollment => enrollment.active)
      .map(r => r.slug);

    Assert.equal(
      activeSlugs.length,
      expected.length,
      `Should be enrolled in ${expected.length} experiments and rollouts`
    );

    for (const { slug, isRollout } of expected) {
      Assert.ok(
        activeSlugs.includes(`optin-${slug}`),
        `Should be enrolled in ${
          isRollout ? "rollout" : "experiment"
        } with slug optin-${slug}`
      );
    }

    for (const { slug } of expected) {
      await manager.unenroll(`optin-${slug}`);
    }
  }

  await cleanup();
});

add_task(async function test_featureIds_is_stored() {
  Services.prefs.setStringPref("messaging-system.log", "all");
  const recipe = NimbusTestUtils.factories.recipe("featureIds");
  // Ensure we get enrolled
  recipe.bucketConfig.count = recipe.bucketConfig.total;

  const { manager, cleanup } = await setupTest();

  const doExperimentCleanup = await NimbusTestUtils.enroll(recipe, {
    manager,
  });

  Assert.ok(manager.store.addEnrollment.calledOnce, "experiment is stored");
  const [enrollment] = manager.store.addEnrollment.firstCall.args;
  Assert.ok("featureIds" in enrollment, "featureIds is stored");
  Assert.deepEqual(
    enrollment.featureIds,
    ["testFeature"],
    "Has expected value"
  );

  await doExperimentCleanup();

  await cleanup();
});

add_task(async function experiment_and_rollout_enroll_and_cleanup() {
  const { manager, cleanup } = await setupTest();

  let doRolloutCleanup = await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: "aboutwelcome",
      value: { enabled: true },
    },
    {
      manager,
      isRollout: true,
    }
  );

  let doExperimentCleanup = await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: "aboutwelcome",
      value: { enabled: true },
    },
    { manager }
  );

  Assert.ok(
    Services.prefs.getBoolPref(`${SYNC_DATA_PREF_BRANCH}aboutwelcome.enabled`)
  );
  Assert.ok(
    Services.prefs.getBoolPref(
      `${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome.enabled`
    )
  );

  await doExperimentCleanup();

  Assert.ok(
    !Services.prefs.getBoolPref(
      `${SYNC_DATA_PREF_BRANCH}aboutwelcome.enabled`,
      false
    )
  );
  Assert.ok(
    Services.prefs.getBoolPref(
      `${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome.enabled`
    )
  );

  await doRolloutCleanup();

  Assert.ok(
    !Services.prefs.getBoolPref(
      `${SYNC_DATA_PREF_BRANCH}aboutwelcome.enabled`,
      false
    )
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      `${SYNC_DEFAULTS_PREF_BRANCH}aboutwelcome.enabled`,
      false
    )
  );

  await cleanup();
});

add_task(async function test_reEnroll() {
  const { manager, cleanup } = await setupTest();

  const experiment = NimbusTestUtils.factories.recipe("experiment");
  const rollout = NimbusTestUtils.factories.recipe("rollout", {
    isRollout: true,
  });

  await manager.enroll(experiment, "test");
  Assert.equal(
    manager.store.getExperimentForFeature("testFeature")?.slug,
    experiment.slug,
    "Should enroll in experiment"
  );

  await manager.enroll(rollout, "test");
  Assert.equal(
    manager.store.getRolloutForFeature("testFeature")?.slug,
    rollout.slug,
    "Should enroll in rollout"
  );

  await manager.unenroll(experiment.slug);
  Assert.ok(
    !manager.store.getExperimentForFeature("testFeature"),
    "Should unenroll from experiment"
  );

  await manager.unenroll(rollout.slug);
  Assert.ok(
    !manager.store.getRolloutForFeature("testFeature"),
    "Should unenroll from rollout"
  );

  await Assert.rejects(
    manager.enroll(experiment, "test", { reenroll: true }, "test"),
    /An experiment with the slug "experiment" already exists/,
    "Should not re-enroll in experiment"
  );

  await manager.enroll(rollout, "test", { reenroll: true }, "test");
  Assert.equal(
    manager.store.getRolloutForFeature("testFeature")?.slug,
    rollout.slug,
    "Should re-enroll in rollout"
  );

  await manager.unenroll(rollout.slug);

  await cleanup();
});

add_task(async function test_randomizationUnit() {
  const ENROLL = "cedc1378-b806-4664-8c3e-2090f2f46e00";
  const NOT_ENROLL = "b502506a-416c-40ea-9f96-c6feaf451470";

  const normandyIdBucketing = {
    ...NimbusTestUtils.factories.recipe.bucketConfig,
    count: 100,
  };
  const groupIdBucketing = {
    ...NimbusTestUtils.factories.recipe.bucketConfig,
    randomizationUnit: "group_id",
    count: 100,
  };

  Services.prefs.setStringPref("app.normandy.user_id", ENROLL);
  await ClientID.setProfileGroupID(NOT_ENROLL);

  Assert.ok(
    await ExperimentAPI.manager.isInBucketAllocation(normandyIdBucketing),
    "in bucketing using normandy_id"
  );
  Assert.ok(
    !(await ExperimentAPI.manager.isInBucketAllocation(groupIdBucketing)),
    "not in bucketing using group_id"
  );

  Services.prefs.setStringPref("app.normandy.user_id", NOT_ENROLL);
  await ClientID.setProfileGroupID(ENROLL);

  Assert.ok(
    !(await ExperimentAPI.manager.isInBucketAllocation(normandyIdBucketing)),
    "not in bucketing using normandy_id"
  );
  Assert.ok(
    await ExperimentAPI.manager.isInBucketAllocation(groupIdBucketing),
    "in bucketing using group_id"
  );
});

add_task(async function test_group_enrollment() {
  const recipe = NimbusTestUtils.factories.recipe("group_enroll", {
    bucketConfig: {
      ...NimbusTestUtils.factories.recipe.bucketConfig,
      randomizationUnit: "group_id",
    },
  });

  await ClientID.setProfileGroupID("cedc1378-b806-4664-8c3e-2090f2f46e00");

  for (const clientID of ["clientid1", "clientid2"]) {
    Services.prefs.setStringPref("app.normandy.user_id", clientID);
    const { manager, cleanup } = await setupTest();

    const enrollment = await manager.enroll(recipe, "test");

    Assert.ok(enrollment.active, "Enrolled in recipe");
    Assert.equal(
      enrollment.branch.slug,
      "treatment",
      "Should have enrolled in the expected branch"
    );

    await manager.unenroll(recipe.slug);

    await cleanup();
  }

  Services.prefs.clearUserPref("app.normandy.user_id");
});

add_task(async function test_getSingleOptInRecipe() {
  const optInRecipes = [
    NimbusTestUtils.factories.recipe("opt-in-one", {
      isRollout: true,
      isFirefoxLabsOptIn: true,
      firefoxLabsTitle: "bogus-title",
      firefoxLabsDescription: "bogus-title",
      firefoxLabsDescriptionLinks: {},
      firefoxLabsGroup: "bogus-group",
      requiresRestart: false,
    }),
    NimbusTestUtils.factories.recipe("opt-in-two", {
      isRollout: true,
      isFirefoxLabsOptIn: true,
      firefoxLabsTitle: "bogus-title",
      firefoxLabsDescription: "bogus-title",
      firefoxLabsDescriptionLinks: {},
      firefoxLabsGroup: "bogus-group",
      requiresRestart: false,
    }),
  ];

  const { loader, manager, cleanup } = await setupTest({
    experiments: optInRecipes,
  });
  await loader.finishedUpdating();

  Assert.deepEqual(
    manager.optInRecipes,
    optInRecipes,
    "Should have recorded opt-in recipes"
  );

  Assert.equal(
    await manager.getSingleOptInRecipe(optInRecipes[0].slug),
    optInRecipes[0],
    "should return the correct opt in recipe with the slug opt-in-one"
  );

  Assert.equal(
    await manager.getSingleOptInRecipe("non-existent"),
    undefined,
    "should return undefined if no opt in recipe exists with the slug non-existent"
  );

  await Assert.rejects(
    manager.getSingleOptInRecipe(),
    /Slug required for .getSingleOptInRecipe/,
    "Should throw when .getSingleOptInRecipe is called without a slug argument"
  );

  await cleanup();
});

add_task(async function test_getAllOptInRecipes() {
  const recipes = [
    NimbusTestUtils.factories.recipe("match-1", {
      isRollout: true,
      isFirefoxLabsOptIn: true,
      firefoxLabsTitle: "bogus-title",
      firefoxLabsDescription: "bogus-desc",
      firefoxLabsDescriptionLinks: {},
      firefoxLabsGroup: "bogus-group",
      requiresRestart: false,
    }),
    NimbusTestUtils.factories.recipe("match-2", {
      isRollout: true,
      isFirefoxLabsOptIn: true,
      firefoxLabsTitle: "bogus-title",
      firefoxLabsDescription: "bogus-desc",
      firefoxLabsDescriptionLinks: {},
      firefoxLabsGroup: "bogus-group",
      requiresRestart: false,
    }),
    NimbusTestUtils.factories.recipe("targeting-only-1", {
      bucketConfig: {
        ...NimbusTestUtils.factories.recipe.bucketConfig,
        count: 0,
      },
      isRollout: true,
      isFirefoxLabsOptIn: true,
      firefoxLabsTitle: "bogus-title",
      firefoxLabsDescription: "bogus-desc",
      firefoxLabsDescriptionLinks: {},
      firefoxLabsGroup: "bogus-group",
      requiresRestart: false,
    }),
    NimbusTestUtils.factories.recipe("targeting-only-2", {
      bucketConfig: {
        ...NimbusTestUtils.factories.recipe.bucketConfig,
        count: 0,
      },
      isRollout: true,
      isFirefoxLabsOptIn: true,
      firefoxLabsTitle: "bogus-title",
      firefoxLabsDescription: "bogus-desc",
      firefoxLabsDescriptionLinks: {},
      firefoxLabsGroup: "bogus-group",
      requiresRestart: false,
    }),
    NimbusTestUtils.factories.recipe("bucketing-only-1", {
      targeting: "false",
      isRollout: true,
      isFirefoxLabsOptIn: true,
      firefoxLabsTitle: "bogus-title",
      firefoxLabsDescription: "bogus-desc",
      firefoxLabsDescriptionLinks: {},
      firefoxLabsGroup: "bogus-group",
      requiresRestart: false,
    }),
    NimbusTestUtils.factories.recipe("bucketing-only-2", {
      targeting: "false",
      isRollout: true,
      isFirefoxLabsOptIn: true,
      firefoxLabsTitle: "bogus-title",
      firefoxLabsDescription: "bogus-desc",
      firefoxLabsDescriptionLinks: {},
      firefoxLabsGroup: "bogus-group",
      requiresRestart: false,
    }),
  ];
  const { loader, manager, cleanup } = await setupTest({
    experiments: recipes,
  });
  await loader.finishedUpdating();

  const slugs = await manager
    .getAllOptInRecipes()
    .then(recipes => recipes.map(r => r.slug));

  Assert.deepEqual(
    slugs.sort(),
    ["match-1", "match-2"].sort(),
    "Should only return the matching recipes"
  );

  await cleanup();
});

add_task(async function testCoenrolling() {
  const { manager, cleanup } = await setupTest();

  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout-1",
      { featureId: "no-feature-firefox-desktop" },
      { isRollout: true }
    ),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig(
      "rollout-2",
      { featureId: "no-feature-firefox-desktop" },
      { isRollout: true }
    ),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("experiment-1", {
      featureId: "no-feature-firefox-desktop",
    }),
    "test"
  );
  await manager.enroll(
    NimbusTestUtils.factories.recipe.withFeatureConfig("experiment-2", {
      featureId: "no-feature-firefox-desktop",
    }),
    "test"
  );

  Assert.ok(manager.store.get("rollout-1").active, "rollout-1 is active");
  Assert.ok(manager.store.get("rollout-2").active, "rollout-2 is active");
  Assert.ok(manager.store.get("experiment-1").active, "experiment-1 is active");
  Assert.ok(manager.store.get("experiment-2").active, "experiment-2 is active");

  await manager.unenroll("rollout-1");
  await manager.unenroll("rollout-2");
  await manager.unenroll("experiment-1");
  await manager.unenroll("experiment-2");

  await cleanup();
});
