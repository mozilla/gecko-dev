/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

const FOO_FAKE_FEATURE_MANIFEST = {
  isEarlyStartup: true,
  variables: {
    remoteValue: {
      type: "int",
    },
    enabled: {
      type: "boolean",
    },
  },
};

const BAR_FAKE_FEATURE_MANIFEST = {
  isEarlyStartup: true,
  variables: {
    remoteValue: {
      type: "int",
    },
    enabled: {
      type: "boolean",
    },
  },
};

const REMOTE_CONFIGURATION_FOO =
  NimbusTestUtils.factories.recipe.withFeatureConfig(
    "foo-rollout",
    {
      branchSlug: "foo-rollout-branch",
      featureId: "foo",
      value: { remoteValue: 42, enabled: true },
    },
    { isRollout: true }
  );
const REMOTE_CONFIGURATION_BAR =
  NimbusTestUtils.factories.recipe.withFeatureConfig(
    "bar-rollout",
    {
      branchSlug: "bar-rollout-branch",
      featureId: "bar",
      value: { remoteValue: 3, enabled: true },
    },
    { isRollout: true }
  );

const SYNC_DEFAULTS_PREF_BRANCH = "nimbus.syncdefaultsstore.";

add_setup(function () {
  const sandbox = sinon.createSandbox();

  const client = RemoteSettings("nimbus-desktop-experiments");
  sandbox.stub(client, "get").resolves([]);
  sandbox.stub(client.db, "getLastModified").resolves(0);

  const secureClient = RemoteSettings("nimbus-secure-experiments");
  sandbox.stub(secureClient, "get").resolves([]);
  sandbox.stub(secureClient.db, "getLastModified").resolves(0);

  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

function setup(configuration) {
  const client = RemoteSettings("nimbus-desktop-experiments");
  client.get.resolves(
    configuration ?? [REMOTE_CONFIGURATION_FOO, REMOTE_CONFIGURATION_BAR]
  );
  const secureClient = RemoteSettings("nimbus-secure-experiments");
  secureClient.get.resolves([]);

  // Simulate a state where no experiment exists.
  const cleanup = () => client.get.resolves([]);
  return { client, cleanup };
}

add_task(async function test_remote_fetch_and_ready() {
  const cleanupTestFeatures = NimbusTestUtils.addTestFeatures(
    new ExperimentFeature("foo", FOO_FAKE_FEATURE_MANIFEST),
    new ExperimentFeature("bar", BAR_FAKE_FEATURE_MANIFEST)
  );

  const sandbox = sinon.createSandbox();
  const setExperimentActiveStub = sandbox.stub(
    TelemetryEnvironment,
    "setExperimentActive"
  );
  const setExperimentInactiveStub = sandbox.stub(
    TelemetryEnvironment,
    "setExperimentInactive"
  );

  Assert.equal(
    NimbusFeatures.foo.getVariable("remoteValue"),
    undefined,
    "This prop does not exist before we sync"
  );

  await ExperimentAPI.ready();

  const { cleanup } = setup();

  // Fake being initialized so we can update recipes
  // we don't need to start any timers
  ExperimentAPI._rsLoader._enabled = true;
  await ExperimentAPI._rsLoader.updateRecipes("browser_rsel_remote_defaults");

  Assert.equal(
    NimbusFeatures.foo.getVariable("remoteValue"),
    REMOTE_CONFIGURATION_FOO.branches[0].features[0].value.remoteValue,
    "`foo` feature is set by remote defaults"
  );
  Assert.equal(
    NimbusFeatures.bar.getVariable("remoteValue"),
    REMOTE_CONFIGURATION_BAR.branches[0].features[0].value.remoteValue,
    "`bar` feature is set by remote defaults"
  );

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar`),
    "Pref cache is set"
  );

  // Check if we sent active experiment data for defaults
  Assert.equal(
    setExperimentActiveStub.callCount,
    2,
    "setExperimentActive called once per feature"
  );

  Assert.ok(
    setExperimentActiveStub.calledWith(
      REMOTE_CONFIGURATION_FOO.slug,
      REMOTE_CONFIGURATION_FOO.branches[0].slug,
      {
        type: "nimbus-rollout",
      }
    ),
    "should call setExperimentActive with `foo` feature"
  );
  Assert.ok(
    setExperimentActiveStub.calledWith(
      REMOTE_CONFIGURATION_BAR.slug,
      REMOTE_CONFIGURATION_BAR.branches[0].slug,
      {
        type: "nimbus-rollout",
      }
    ),
    "should call setExperimentActive with `bar` feature"
  );

  // Test Glean experiment API interaction
  Assert.equal(
    Services.fog.testGetExperimentData(REMOTE_CONFIGURATION_FOO.slug).branch,
    REMOTE_CONFIGURATION_FOO.branches[0].slug,
    "Glean.setExperimentActive called with `foo` feature"
  );
  Assert.equal(
    Services.fog.testGetExperimentData(REMOTE_CONFIGURATION_BAR.slug).branch,
    REMOTE_CONFIGURATION_BAR.branches[0].slug,
    "Glean.setExperimentActive called with `bar` feature"
  );

  Assert.equal(
    NimbusFeatures.foo.getVariable("remoteValue"),
    42,
    "Has rollout value"
  );
  Assert.equal(
    NimbusFeatures.bar.getVariable("remoteValue"),
    3,
    "Has rollout value"
  );

  // Clear RS db and load again. No configurations so should clear the cache.
  cleanup();
  await ExperimentAPI._rsLoader.updateRecipes("browser_rsel_remote_defaults");

  Assert.ok(
    !NimbusFeatures.foo.getVariable("remoteValue"),
    "foo-rollout should be removed"
  );
  Assert.ok(
    !NimbusFeatures.bar.getVariable("remoteValue"),
    "bar-rollout should be removed"
  );

  // Check if we sent active experiment data for defaults
  Assert.equal(
    setExperimentInactiveStub.callCount,
    2,
    "setExperimentInactive called once per feature"
  );

  Assert.ok(
    setExperimentInactiveStub.calledWith(REMOTE_CONFIGURATION_FOO.slug),
    "should call setExperimentInactive with `foo` feature"
  );
  Assert.ok(
    setExperimentInactiveStub.calledWith(REMOTE_CONFIGURATION_BAR.slug),
    "should call setExperimentInactive with `bar` feature"
  );

  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar`, ""),
    "Should clear the pref"
  );
  Assert.ok(
    !NimbusFeatures.bar.getVariable("remoteValue"),
    "Should be missing"
  );

  ExperimentAPI.manager.store._deleteForTests("foo");
  ExperimentAPI.manager.store._deleteForTests("bar");
  ExperimentAPI.manager.store._deleteForTests(REMOTE_CONFIGURATION_FOO.slug);
  ExperimentAPI.manager.store._deleteForTests(REMOTE_CONFIGURATION_BAR.slug);
  sandbox.restore();

  cleanup();
  cleanupTestFeatures();
});

add_task(async function test_remote_fetch_on_updateRecipes() {
  let sandbox = sinon.createSandbox();
  let updateRecipesStub = sandbox.stub(
    ExperimentAPI._rsLoader,
    "updateRecipes"
  );
  // Work around the pref change callback that would trigger `setTimer`
  sandbox.replaceGetter(ExperimentAPI._rsLoader, "intervalInSeconds", () => 1);

  // This will un-register the timer
  ExperimentAPI._rsLoader._enabled = true;
  ExperimentAPI._rsLoader.disable();
  Services.prefs.clearUserPref(
    "app.update.lastUpdateTime.rs-experiment-loader-timer"
  );

  ExperimentAPI._rsLoader._enabled = true;
  ExperimentAPI._rsLoader.setTimer();

  await BrowserTestUtils.waitForCondition(
    () => updateRecipesStub.called,
    "Wait for timer to call"
  );

  Assert.ok(updateRecipesStub.calledOnce, "Timer calls function");
  Assert.equal(updateRecipesStub.firstCall.args[0], "timer", "Called by timer");
  sandbox.restore();
  // This will un-register the timer
  ExperimentAPI._rsLoader.disable();
  Services.prefs.clearUserPref(
    "app.update.lastUpdateTime.rs-experiment-loader-timer"
  );
});

add_task(async function test_finalizeRemoteConfigs_cleanup() {
  const cleanupTestFeatures = NimbusTestUtils.addTestFeatures(
    new ExperimentFeature("foo", {
      description: "mochitests",
      isEarlyStartup: true,
      variables: {
        foo: { type: "boolean" },
      },
    }),
    new ExperimentFeature("bar", {
      description: "mochitests",
      isEarlyStartup: true,
      variables: {
        bar: { type: "boolean" },
      },
    })
  );

  let fooCleanup = await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: "foo",
      value: { foo: true },
    },
    {
      slug: "foo-rollout",
      branchSlug: REMOTE_CONFIGURATION_FOO.branches[0].slug,
      isRollout: true,
      source: "rs-loader",
    }
  );
  await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: "bar",
      value: { bar: true },
    },
    {
      slug: "bar-rollout",
      branchSlug: REMOTE_CONFIGURATION_BAR.branches[0].slug,
      isRollout: true,
      source: "rs-loader",
    }
  );
  let stubFoo = sinon.stub();
  let stubBar = sinon.stub();
  NimbusFeatures.foo.onUpdate(stubFoo);
  NimbusFeatures.bar.onUpdate(stubBar);

  // stubFoo and stubBar will be called because the store is ready. We are not interested in these calls.
  // Reset call history and check calls stats after cleanup.
  Assert.ok(
    stubFoo.called,
    "feature foo update triggered becuase store is ready"
  );
  Assert.ok(
    stubBar.called,
    "feature bar update triggered because store is ready"
  );
  stubFoo.resetHistory();
  stubBar.resetHistory();

  Services.prefs.setStringPref(
    `${SYNC_DEFAULTS_PREF_BRANCH}foo`,
    JSON.stringify({ foo: true, branch: { feature: { featureId: "foo" } } })
  );
  Services.prefs.setStringPref(
    `${SYNC_DEFAULTS_PREF_BRANCH}bar`,
    JSON.stringify({ bar: true, branch: { feature: { featureId: "bar" } } })
  );

  const remoteConfiguration = {
    ...REMOTE_CONFIGURATION_FOO,
    branches: [
      {
        ...REMOTE_CONFIGURATION_FOO.branches[0],
        features: [
          {
            ...REMOTE_CONFIGURATION_FOO.branches[0].features[0],
            value: {
              foo: true,
            },
          },
        ],
      },
    ],
  };

  const { cleanup } = await setup([remoteConfiguration]);
  ExperimentAPI._rsLoader._enabled = true;
  await ExperimentAPI._rsLoader.updateRecipes();

  Assert.ok(
    stubFoo.notCalled,
    "Not called, not enrolling in rollout feature already exists"
  );
  Assert.ok(stubBar.called, "Called because no recipe is seen, cleanup");
  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}foo`),
    "Pref is not cleared"
  );
  Assert.ok(
    !Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar`, ""),
    "Pref was cleared"
  );

  await fooCleanup();
  // This will also remove the inactive recipe from the store
  // the previous update (from recipe not seen code path)
  // only sets the recipe as inactive
  ExperimentAPI.manager.store._deleteForTests("bar-rollout");
  ExperimentAPI.manager.store._deleteForTests("foo-rollout");

  cleanupTestFeatures();
  cleanup();
});

// If the remote config data returned from the store is not modified
// this test should not throw
add_task(async function remote_defaults_no_mutation() {
  let sandbox = sinon.createSandbox();
  sandbox.stub(ExperimentAPI.manager.store, "getRolloutForFeature").returns(
    Cu.cloneInto(
      {
        featureIds: ["foo"],
        branch: {
          features: [{ featureId: "foo", value: { remoteStub: true } }],
        },
      },
      {},
      { deepFreeze: true }
    )
  );

  let fooInstance = new ExperimentFeature("foo", FOO_FAKE_FEATURE_MANIFEST);
  let config = fooInstance.getAllVariables();

  Assert.ok(config.remoteStub, "Got back the expected value");

  sandbox.restore();
});

add_task(async function remote_defaults_active_remote_defaults() {
  ExperimentAPI.manager.store._deleteForTests("foo");
  ExperimentAPI.manager.store._deleteForTests("bar");
  let barFeature = new ExperimentFeature("bar", {
    description: "mochitest",
    variables: { enabled: { type: "boolean" } },
  });
  let fooFeature = new ExperimentFeature("foo", {
    description: "mochitest",
    variables: { enabled: { type: "boolean" } },
  });

  const cleanupTestFeatures = NimbusTestUtils.addTestFeatures(
    barFeature,
    fooFeature
  );

  let rollout1 = NimbusTestUtils.factories.recipe.withFeatureConfig(
    "bar",
    {
      branchSlug: "bar-rollout-branch",
      featureId: "bar",
      value: { enabled: true },
    },
    { isRollout: true, targeting: "true" }
  );

  let rollout2 = NimbusTestUtils.factories.recipe.withFeatureConfig(
    "foo",
    {
      branchSlug: "foo-rollout-branch",
      featureId: "foo",
      value: { enabled: true },
    },
    { isRollout: true, targeting: "'bar' in activeRollouts" }
  );

  // Order is important, rollout2 won't match at first
  const { cleanup } = setup([rollout2, rollout1]);
  let updatePromise = new Promise(resolve => barFeature.onUpdate(resolve));
  ExperimentAPI._rsLoader._enabled = true;
  await ExperimentAPI._rsLoader.updateRecipes("mochitest");

  await updatePromise;

  Assert.ok(barFeature.getVariable("enabled"), "Enabled on first sync");
  Assert.ok(!fooFeature.getVariable("enabled"), "Targeting doesn't match");

  let featureUpdate = new Promise(resolve => fooFeature.onUpdate(resolve));
  await ExperimentAPI._rsLoader.updateRecipes("mochitest");
  await featureUpdate;

  Assert.ok(fooFeature.getVariable("enabled"), "Targeting should match");

  await NimbusTestUtils.cleanupManager(["foo", "bar"]);
  ExperimentAPI.manager.store._deleteForTests("foo");
  ExperimentAPI.manager.store._deleteForTests("bar");

  cleanup();
  cleanupTestFeatures();
});

add_task(async function remote_defaults_variables_storage() {
  let barFeature = new ExperimentFeature("bar", {
    description: "mochitest",
    isEarlyStartup: true,
    variables: {
      enabled: {
        type: "boolean",
      },
      storage: {
        type: "int",
      },
      object: {
        type: "json",
      },
      string: {
        type: "string",
      },
      bool: {
        type: "boolean",
      },
    },
  });
  let rolloutValue = {
    storage: 42,
    object: { foo: "foo" },
    string: "string",
    bool: true,
    enabled: true,
  };

  // TODO(bug 1959831): isEarlyStartup is checked directly on the feature
  // manifest, not NimbusFeatures.
  const featureCleanup = NimbusTestUtils.addTestFeatures(barFeature);

  let doCleanup = await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: "bar",
      value: rolloutValue,
    },
    { isRollout: true }
  );

  Assert.ok(
    Services.prefs.getStringPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar`, ""),
    "Experiment stored in prefs"
  );
  Assert.ok(
    Services.prefs.getIntPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar.storage`, 0),
    "Stores variable in separate pref"
  );
  Assert.equal(
    Services.prefs.getIntPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar.storage`, 0),
    42,
    "Stores variable in correct type"
  );
  Assert.deepEqual(
    barFeature.getAllVariables(),
    rolloutValue,
    "Test types are returned correctly"
  );

  await doCleanup();

  Assert.equal(
    Services.prefs.getIntPref(`${SYNC_DEFAULTS_PREF_BRANCH}bar.storage`, -1),
    -1,
    "Variable pref is cleared"
  );
  Assert.ok(!barFeature.getVariable("string"), "Variable is no longer defined");
  ExperimentAPI.manager.store._deleteForTests("bar");
  ExperimentAPI.manager.store._deleteForTests("bar-rollout");

  delete NimbusFeatures.bar;
  featureCleanup();
});
