/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { FirefoxLabs } = ChromeUtils.importESModule(
  "resource://nimbus/FirefoxLabs.sys.mjs"
);

function setupTest({ ...ctx }) {
  return NimbusTestUtils.setupTest({ ...ctx, clearTelemetry: true });
}

add_setup(function () {
  Services.fog.initializeFOG();
});

add_task(async function test_all() {
  const { sandbox, manager, initExperimentAPI, cleanup } = await setupTest({
    experiments: [
      NimbusTestUtils.factories.recipe("opt-in-rollout", {
        isRollout: true,
        isFirefoxLabsOptIn: true,
        firefoxLabsTitle: "title",
        firefoxLabsDescription: "description",
        firefoxLabsDescriptionLinks: null,
        firefoxLabsGroup: "group",
        requiresRestart: false,
      }),
      NimbusTestUtils.factories.recipe("opt-in-experiment", {
        branches: [
          {
            ...NimbusTestUtils.factories.recipe.branches[0],
            firefoxLabsTitle: "title",
          },
        ],
        isFirefoxLabsOptIn: true,
        firefoxLabsTitle: "title",
        firefoxLabsDescription: "description",
        firefoxLabsDescriptionLinks: null,
        firefoxLabsGroup: "group",
        requiresRestart: false,
      }),
      NimbusTestUtils.factories.recipe("targeting-fail", {
        targeting: "false",
        isRollout: true,
        isFirefoxLabsOptIn: true,
        firefoxLabsTitle: "title",
        firefoxLabsDescription: "description",
        firefoxLabsDescriptionLinks: null,
        firefoxLabsGroup: "group",
        requiresRestart: false,
      }),
      NimbusTestUtils.factories.recipe("bucketing-fail", {
        bucketConfig: {
          ...NimbusTestUtils.factories.recipe.bucketConfig,
          count: 0,
        },
        isRollout: true,
        isFirefoxLabsOptIn: true,
        firefoxLabsTitle: "title",
        firefoxLabsDescription: "description",
        firefoxLabsDescriptionLinks: null,
        firefoxLabsGroup: "group",
        requiresRestart: false,
      }),
      NimbusTestUtils.factories.recipe("experiment"),
      NimbusTestUtils.factories.recipe("rollout", { isRollout: true }),
    ],
    init: false,
  });

  // Stub out enrollment because we don't care.
  sandbox.stub(manager, "enroll");

  await initExperimentAPI();

  const labs = await FirefoxLabs.create();
  const availableSlugs = Array.from(labs.all(), recipe => recipe.slug).sort();

  Assert.deepEqual(
    availableSlugs,
    ["opt-in-rollout", "opt-in-experiment"].sort(),
    "Should return all opt in recipes that match targeting and bucketing"
  );

  await cleanup();
});

add_task(async function test_enroll() {
  const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
    "opt-in",
    { featureId: "nimbus-qa-1" },
    {
      isRollout: true,
      isFirefoxLabsOptIn: true,
      firefoxLabsTitle: "placeholder",
      firefoxLabsDescription: "placeholder",
      firefoxLabsDescriptionLinks: null,
      firefoxLabsGroup: "placeholder",
      requiresRestart: false,
    }
  );

  const { sandbox, manager, initExperimentAPI, cleanup } = await setupTest({
    experiments: [recipe],
    init: false,
  });

  const enrollSpy = sandbox.spy(manager, "enroll");

  await initExperimentAPI();

  const labs = await FirefoxLabs.create();

  await Assert.rejects(
    labs.enroll(),
    /enroll: slug and branchSlug are required/,
    "Should throw when enroll() is called without a slug"
  );

  await Assert.rejects(
    labs.enroll("opt-in"),
    /enroll: slug and branchSlug are required/,
    "Should throw when enroll() is called without a branch slug"
  );

  await labs.enroll("bogus", "bogus");
  Assert.ok(
    enrollSpy.notCalled,
    "ExperimentManager.enroll not called for unknown recipe"
  );

  await labs.enroll(recipe.slug, "bogus");
  Assert.ok(
    enrollSpy.notCalled,
    "ExperimentManager.enroll not called for invalid branch slug"
  );

  await labs.enroll(recipe.slug, recipe.branches[0].slug);
  Assert.ok(
    enrollSpy.calledOnceWith(recipe, "rs-loader", { branchSlug: "control" }),
    "ExperimentManager.enroll called"
  );

  Assert.deepEqual(
    Glean.nimbusEvents.enrollmentStatus
      .testGetValue("events")
      ?.map(ev => ev.extra),
    [
      {
        slug: recipe.slug,
        branch: "control",
        status: "Enrolled",
        reason: "OptIn",
      },
    ]
  );

  Assert.ok(manager.store.get(recipe.slug)?.active, "Active enrollment exists");

  await labs.unenroll(recipe.slug);

  await cleanup();
});

add_task(async function test_reenroll() {
  const recipe = NimbusTestUtils.factories.recipe("opt-in", {
    isFirefoxLabsOptIn: true,
    firefoxLabsTitle: "placeholder",
    firefoxLabsDescription: "placeholder",
    firefoxLabsDescriptionLinks: null,
    firefoxLabsGroup: "placeholder",
    requiresRestart: false,
    isRollout: true,
  });

  const { manager, cleanup } = await setupTest({ experiments: [recipe] });

  const labs = await FirefoxLabs.create();

  Assert.ok(
    typeof manager.store.get(recipe.slug) === "undefined",
    `No enrollment for ${recipe.slug}`
  );

  await labs.enroll(recipe.slug, "control");
  Assert.ok(
    manager.store.get(recipe.slug)?.active,
    `Active enrollment for ${recipe.slug}`
  );

  await labs.unenroll(recipe.slug);
  Assert.ok(
    manager.store.get(recipe.slug)?.active === false,
    `Inactive enrollment for ${recipe.slug}`
  );

  await ExperimentAPI._rsLoader.updateRecipes();
  Assert.ok(
    manager.store.get(recipe.slug)?.active === false,
    `Inactive enrollment for ${recipe.slug} after updateRecipes()`
  );

  await labs.enroll(recipe.slug, "control");
  Assert.ok(
    manager.store.get(recipe.slug)?.active,
    `Active enrollment for ${recipe.slug}`
  );

  await labs.unenroll(recipe.slug);

  await cleanup();
});

add_task(async function test_unenroll() {
  const { manager, cleanup } = await setupTest({
    experiments: [
      NimbusTestUtils.factories.recipe.withFeatureConfig(
        "rollout",
        { featureId: "nimbus-qa-1" },
        { isRollout: true }
      ),
      NimbusTestUtils.factories.recipe.withFeatureConfig(
        "opt-in",
        { featureId: "nimbus-qa-2" },
        {
          isRollout: true,
          isFirefoxLabsOptIn: true,
          firefoxLabsTitle: "title",
          firefoxLabsDescription: "description",
          firefoxLabsDescriptionLinks: null,
          firefoxLabsGroup: "group",
          requiresRestart: false,
        }
      ),
    ],
  });

  const labs = await FirefoxLabs.create();

  Assert.ok(manager.store.get("rollout")?.active, "Enrolled in rollout");
  Assert.ok(
    typeof manager.store.get("opt-in") === "undefined",
    "Did not enroll in rollout"
  );

  await labs.enroll("opt-in", "control");
  Assert.ok(manager.store.get("opt-in")?.active, "Enrolled in opt-in");

  // Should not throw.
  await labs.unenroll("bogus");

  // Should not throw.
  await labs.unenroll("rollout");
  Assert.ok(
    manager.store.get("rollout").active,
    "Enrolled in rollout after attempting to unenroll with incorrect API"
  );

  await labs.unenroll("opt-in");
  Assert.ok(!manager.store.get("opt-in").active, "Unenrolled from opt-in");

  // Should not throw.
  await labs.unenroll("opt-in");

  Assert.deepEqual(
    Glean.nimbusEvents.enrollmentStatus
      .testGetValue("events")
      ?.map(ev => ev.extra),
    [
      {
        branch: "control",
        status: "Enrolled",
        slug: "rollout",
        reason: "Qualified",
      },
      {
        status: "Enrolled",
        slug: "opt-in",
        reason: "OptIn",
        branch: "control",
      },
      {
        branch: "control",
        reason: "OptOut",
        slug: "opt-in",
        status: "Disqualified",
      },
    ]
  );

  await manager.unenroll("rollout");
  await cleanup();
});
