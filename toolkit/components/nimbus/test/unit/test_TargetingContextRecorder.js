/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { NewTabUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/NewTabUtils.sys.mjs"
);

const {
  ATTRIBUTE_TRANSFORMS,
  PREFS,
  recordTargetingContext,
  normalizeAttributeName,
  normalizePrefName,
} = ChromeUtils.importESModule(
  "resource://nimbus/lib/TargetingContextRecorder.sys.mjs"
);

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const { ExtensionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionXPCShellUtils.sys.mjs"
);

AddonTestUtils.init(this);
ExtensionTestUtils.init(this);

const TARGETING_CONTEXT_METRICS = Object.keys(ATTRIBUTE_TRANSFORMS).map(
  normalizeAttributeName
);

async function setupTest({ ...args } = {}) {
  const { cleanup: baseCleanup, ...ctx } = await NimbusTestUtils.setupTest({
    ...args,
    clearTelemetry: true,
  });

  const localeService = Services.locale;
  const mockLocaleService = new Proxy(localeService, {
    get(obj, prop) {
      if (prop === "appLocaleAsBCP47") {
        return "en-US";
      }

      return obj[prop];
    },
  });

  Services.locale = mockLocaleService;

  return {
    ...ctx,
    async cleanup() {
      await baseCleanup();
      Services.locale = localeService;
    },
  };
}

/**
 * Return an object containing each nimbus_targeting_context metric mapped to
 * its recorded value.
 *
 * @returns {object} A mapping of metric names to their values.
 */
function getRecordedTargetingContextMetrics() {
  const values = {};
  for (const metric of TARGETING_CONTEXT_METRICS) {
    const value = Glean.nimbusTargetingContext[metric].testGetValue();

    if (value !== null) {
      values[metric] = value;
    }
  }
  return values;
}

/**
 * Assert there were the expected errors when recording the Nimbus targeting
 * context.
 *
 * @param {object} expectedErrors
 * @param {string[]} expectedErrors.attrEvalErrors
 *        Attributes that should have failed to record in the
 *        nimbus_targeting_context metrics and should appear in the
 *        nimbus_targeting_environment.attr_eval_errors metric.
 * @param {string[]} expectedErrors.prefTypeErrors
 *        Preferences that should have failed to record in the
 *        nimbus_targeting_environment.pref_values metric and should appear in
 *        the nimbus_targeting_environment.pref_type_errors metric.
 */
function assertRecordingFailures({
  attrEvalErrors = [],
  prefTypeErrors = [],
} = {}) {
  // userSetPrefs should record no errors, so this should not throw.
  Glean.nimbusTargetingEnvironment.userSetPrefs.testGetValue();

  const prefValues = Glean.nimbusTargetingEnvironment.prefValues.testGetValue();
  for (const pref of Object.keys(PREFS)) {
    const errorCount =
      Glean.nimbusTargetingEnvironment.prefTypeErrors[pref].testGetValue() ?? 0;

    if (prefTypeErrors.includes(pref)) {
      Assert.ok(errorCount > 0, `An type error was reported for pref ${pref}`);
      Assert.ok(
        !Object.hasOwn(prefValues, normalizePrefName(pref)),
        `The pref ${pref} should not be recorded`
      );
    } else {
      Assert.equal(errorCount, 0, `An error was not reported for pref ${pref}`);
    }
  }

  const targetingContextMetrics = getRecordedTargetingContextMetrics();
  for (const attr of Object.keys(ATTRIBUTE_TRANSFORMS)) {
    const errorCount =
      Glean.nimbusTargetingEnvironment.attrEvalErrors[attr].testGetValue() ?? 0;

    if (attrEvalErrors.includes(attr)) {
      Assert.ok(errorCount > 0, `An error was reported for attribute ${attr}`);
      Assert.ok(
        !Object.hasOwn(targetingContextMetrics, normalizeAttributeName(attr)),
        `The attribute ${attr} should not have been recorded`
      );
    } else {
      Assert.equal(
        errorCount,
        0,
        `An error was not reported for attribute ${attr}`
      );
    }
  }
}

add_setup(async function test_setup() {
  Services.fog.initializeFOG();
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function testAttributeTransforms() {
  info(
    "testing all attributes in ATTRIBUTE_TRANSFORMS have callable transforms"
  );
  for (const [attribute, transform] of Object.entries(ATTRIBUTE_TRANSFORMS)) {
    Assert.ok(
      typeof transform === "function",
      `Attribute ${attribute} has a callable transform`
    );
  }
});

add_task(async function testNimbusTargetingContextAllKeysPresent() {
  info(
    "testing nimbus_targeting_context metrics contain all keys in the Nimbus targeting context"
  );

  const { cleanup, manager, sandbox } = await setupTest();

  // Glean doesn't serialize empty arrays, so lets put some entries into activeExperiments and
  // activeRollouts so that they appear in the context.
  manager.store.set(
    "experiment",
    NimbusTestUtils.factories.experiment("experiment")
  );
  manager.store.set("rollout", NimbusTestUtils.factories.rollout("rollout"));

  // Stub this for userMonthlyActivity
  sandbox
    .stub(NewTabUtils.activityStreamProvider, "getUserMonthlyActivity")
    .returns(Promise.resolve([[1, "1960-01-01"]]));

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertRecordingFailures();

    const values = getRecordedTargetingContextMetrics();

    Assert.ok(
      Object.keys(values).length !== 0,
      "nimbusTargetingContext metrics were recorded"
    );

    for (const metric of TARGETING_CONTEXT_METRICS) {
      Assert.ok(
        Object.hasOwn(values, metric),
        `nimbusTargetingContext.${metric} was recorded ${JSON.stringify(values[metric])}`
      );
    }
  }, recordTargetingContext);

  manager.store._deleteForTests("experiment");
  manager.store._deleteForTests("rollout");

  await cleanup();
});

add_task(async function testNimbusTargetingEnvironmentUserSetPrefs() {
  info("testing nimbus.targetingContext.pref_is_user_set");

  const { cleanup } = await setupTest();

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertRecordingFailures();

    const prefs = Glean.nimbusTargetingEnvironment.userSetPrefs.testGetValue();
    Assert.ok(
      !prefs.includes("nimbus.testing.testInt"),
      "nimbus.testing.testInt is not set and not in telemetry"
    );
    Assert.ok(
      !prefs.includes("nimbus.testing.testSetString"),
      "nimbus.testing.testInt is not set and not in telemetry"
    );
  }, recordTargetingContext);

  // This pref is a fallbackPref, so should not appear in the list.
  Services.prefs.setIntPref("nimbus.testing.testInt", 123);

  // These two prefs are setPref, and so should appear in the list.
  Services.prefs.setStringPref("nimbus.testing.testSetString", "test");

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertRecordingFailures();

    const prefs = Glean.nimbusTargetingEnvironment.userSetPrefs.testGetValue();

    Assert.ok(
      !prefs.includes("nimbus.testing.testInt"),
      "nimbus.testing.testInt is set and not in telemetry"
    );
    Assert.ok(
      prefs.includes("nimbus.testing.testSetString"),
      "nimbus.testing.testSetString is set and in telemetry"
    );
  }, recordTargetingContext);

  Services.prefs.deleteBranch("nimbus.testing.testInt");
  Services.prefs.deleteBranch("nimbus.testing.testSetString");

  await cleanup();
});

add_task(async function testNimbusTargetingEnvironmentPrefValues() {
  info("testing nimbus.targetingContext.pref_values collects pref values");

  const { cleanup } = await setupTest();
  const PREF = "messaging-system-action.testday";
  const PREF_KEY = "messaging_system_action__testday";

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertRecordingFailures();

    const prefs = Glean.nimbusTargetingEnvironment.prefValues.testGetValue();
    Assert.ok(
      !Object.hasOwn(prefs, PREF_KEY),
      `${PREF} not set and not present in telemetry`
    );
  }, recordTargetingContext);

  Services.prefs.getDefaultBranch(null).setStringPref(PREF, "default");

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertRecordingFailures();

    const prefs = Glean.nimbusTargetingEnvironment.prefValues.testGetValue();
    Assert.equal(
      prefs[PREF_KEY],
      "default",
      `${PREF} set on the default branch and present in telemetry`
    );
  }, recordTargetingContext);

  Services.prefs.setStringPref(PREF, "user");

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertRecordingFailures();

    const prefs = Glean.nimbusTargetingEnvironment.prefValues.testGetValue();
    Assert.equal(
      prefs[PREF_KEY],
      "user",
      `${PREF} set on the user branch and present in telemetry`
    );
  }, recordTargetingContext);

  Services.prefs.deleteBranch(PREF);

  await cleanup();
});

add_task(async function testExperimentMetrics() {
  info(
    "testing values.activeExperiments, values.activeEnrollments, and values.enrollmentsMap"
  );

  const { cleanup, manager } = await setupTest();

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertRecordingFailures();

    const values = getRecordedTargetingContextMetrics();

    Assert.deepEqual(values.activeExperiments, []);
    Assert.deepEqual(values.activeRollouts, []);
    Assert.deepEqual(values.enrollmentsMap, []);
  }, recordTargetingContext);

  manager.store.set(
    "experiment-1",
    NimbusTestUtils.factories.experiment("experiment-1", {
      branch: NimbusTestUtils.factories.recipe.branches[0],
    })
  );
  manager.store.set(
    "experiment-2",
    NimbusTestUtils.factories.experiment("experiment-2", {
      branch: NimbusTestUtils.factories.recipe.branches[1],
    })
  );
  manager.store.set(
    "rollout-1",
    NimbusTestUtils.factories.rollout("rollout-1", {
      branch: {
        ...NimbusTestUtils.factories.recipe.branches[0],
        slug: "rollout",
      },
    })
  );

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertRecordingFailures();

    const values = getRecordedTargetingContextMetrics();

    Assert.deepEqual(values.activeExperiments.sort(), [
      "experiment-1",
      "experiment-2",
    ]);
    Assert.deepEqual(values.activeRollouts, ["rollout-1"]);
    Assert.deepEqual(
      values.enrollmentsMap.sort(),
      [
        { experimentSlug: "experiment-1", branchSlug: "control" },
        { experimentSlug: "experiment-2", branchSlug: "treatment" },
        { experimentSlug: "rollout-1", branchSlug: "rollout" },
      ].sort()
    );
  }, recordTargetingContext);

  manager.store.deactivateEnrollment("experiment-1");
  manager.store.deactivateEnrollment("experiment-2");
  manager.store.deactivateEnrollment("rollout-1");

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertRecordingFailures();

    const values = getRecordedTargetingContextMetrics();

    Assert.deepEqual(values.activeExperiments, []);
    Assert.deepEqual(values.activeRollouts, []);
    Assert.deepEqual(
      values.enrollmentsMap.sort(),
      [
        { experimentSlug: "experiment-1", branchSlug: "control" },
        { experimentSlug: "experiment-2", branchSlug: "treatment" },
        { experimentSlug: "rollout-1", branchSlug: "rollout" },
      ].sort()
    );
  }, recordTargetingContext);

  manager.store._deleteForTests("experiment-1");
  manager.store._deleteForTests("experiment-2");
  manager.store._deleteForTests("rollout-1");

  await cleanup();
});

add_task(async function testErrorMetrics() {
  info(
    "testing nimbus_targeting_environment.{attr_eval_errors,pref_type_errors} telemetry"
  );

  const { cleanup, manager, sandbox } = await setupTest();
  const PREF = "messaging-system-action.testday";
  const PREF_KEY = "messaging_system_action__testday";

  Assert.ok(
    !Services.prefs.prefHasUserValue(PREF),
    `${PREF} not set on user branch`
  );
  Assert.ok(
    !Services.prefs.prefHasDefaultValue(PREF),
    `${PREF} not set on default branch`
  );

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertRecordingFailures();

    const prefs = Glean.nimbusTargetingEnvironment.prefValues.testGetValue();
    Assert.ok(
      !Object.hasOwn(prefs, PREF_KEY),
      `${PREF_KEY} not set and not present in telemetry`
    );
  }, recordTargetingContext);

  info(
    "testing prefs with the wrong type are recorded in the pref_type_errors metric"
  );

  Services.prefs.setIntPref(PREF, 123);

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertRecordingFailures({
      prefTypeErrors: [PREF],
    });

    const prefs = Glean.nimbusTargetingEnvironment.prefValues.testGetValue();
    Assert.ok(
      !Object.hasOwn(prefs, PREF_KEY),
      "nimbus.qa.pref-1 not set and not present in telemetry"
    );
  }, recordTargetingContext);

  Services.prefs.deleteBranch(PREF);

  info(
    "testing values from the context that throw are recorded in the attr_eval_errors metric"
  );

  sandbox.stub(manager, "createTargetingContext").callsFake(function () {
    return {
      isFirstStartup: "invalid",
      activeExperiments: [],
      activeRollouts: [],
      enrollmentsMap: {},
      get currentDate() {
        throw new Error("uh oh");
      },
    };
  });

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertRecordingFailures({
      attrEvalErrors: ["currentDate", "isFirstStartup"],
    });
  }, recordTargetingContext);

  await cleanup();

  Services.prefs.deleteBranch(PREF);
});

add_task(async function testRecordingErrors() {
  info("testing failures recording nimbus_targeting_context metrics");

  const { cleanup, manager, sandbox } = await setupTest();

  sandbox.stub(manager, "createTargetingContext").callsFake(function () {
    return {
      activeExperiments: [1, 2, 3],
      activeRollouts: [4, 5, 6],
      enrollmentsMap: { foo: 1, bar: 2 },
    };
  });

  function assertMetricErrors() {
    for (const metric of [
      "activeExperiments",
      "activeRollouts",
      "enrollmentsMap",
    ]) {
      try {
        console.log(
          "metric value",
          Glean.nimbusTargetingContext[metric].testGetValue()
        );
      } catch (ex) {}

      Assert.throws(
        () => Glean.nimbusTargetingContext[metric].testGetValue(),
        /Metric had 1 error\(s\) of type invalid_value/,
        `There should be Glean error for metric ${metric}`
      );
    }
  }

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertMetricErrors();

    Assert.equal(
      Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue(),
      null,
      "The targetingContextValue metric is not recorded by default."
    );
  }, recordTargetingContext);

  // We triggered glean to record error metrics. Ensure that we don't double count.
  Services.fog.testResetFOG();

  // In the real world this would be done via the nimbusTelemetry feature.
  Services.fog.applyServerKnobsConfig(
    JSON.stringify({
      metrics_enabled: {
        "nimbus_targeting_environment.targeting_context_value": true,
      },
    })
  );

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    assertMetricErrors();

    const stringifiedCtx =
      Glean.nimbusTargetingEnvironment.targetingContextValue.testGetValue();
    Assert.ok(
      typeof stringifiedCtx === "string",
      "The targetingContextValue metric is recorded"
    );

    const context = JSON.parse(stringifiedCtx);
    Assert.ok(
      Object.hasOwn(context, "activeExperiments"),
      "activeExperiments should be recorded in targetingContextValue"
    );
    Assert.deepEqual(
      context.activeExperiments,
      [1, 2, 3],
      "activeExperiments should have the invalid value in the targetingContextValue metric"
    );
    Assert.ok(
      Object.hasOwn(context, "activeRollouts"),
      "activeRollouts should be recorded in targetingContextValue"
    );
    Assert.deepEqual(
      context.activeRollouts,
      [4, 5, 6],
      "activeExperiments should have the invalid value in the targetingContextValue metric"
    );
    Assert.ok(
      Object.hasOwn(context, "enrollmentsMap"),
      "enrollmentsMap should be recorded in targetingContextValue"
    );
    Assert.deepEqual(
      context.enrollmentsMap,
      [
        {
          experimentSlug: "foo",
          branchSlug: 1,
        },
        {
          experimentSlug: "bar",
          branchSlug: 2,
        },
      ],
      "activeExperiments should have the invalid value in the targetingContextValue metric"
    );
  }, recordTargetingContext);

  await cleanup();

  // We applied server knobs config and triggered Glean recording errors.
  Services.fog.testResetFOG();
});

add_task(async function testAddonsInfo() {
  const { cleanup } = await setupTest();

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    const values = getRecordedTargetingContextMetrics();

    Assert.ok(
      Object.hasOwn(values, "addonsInfo"),
      "addonsInfo in targeting Context"
    );
    Assert.equal(
      values.addonsInfo.hasInstalledAddons,
      false,
      "hasInstalledAddons is false"
    );
    Assert.deepEqual(
      values.addonsInfo.addons ?? [],
      [],
      "No recorded addon info"
    );
  }, recordTargetingContext);

  const ext1 = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      manifest_version: 2,
      name: "test-addon",
      version: "1.0",
    },
  });

  await ext1.startup();
  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    const values = getRecordedTargetingContextMetrics();

    Assert.ok(
      Object.hasOwn(values, "addonsInfo"),
      "addonsInfo in targeting Context"
    );
    Assert.equal(
      values.addonsInfo.hasInstalledAddons,
      true,
      "hasInstalledAddons is true"
    );
    Assert.deepEqual(values.addonsInfo.addons, [ext1.id], "Has one addon");
  }, recordTargetingContext);

  const ext2 = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      manifest_version: 2,
      name: "test-addon-2",
      version: "2.0",
    },
  });
  await ext2.startup();

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    const values = getRecordedTargetingContextMetrics();

    Assert.ok(
      Object.hasOwn(values, "addonsInfo"),
      "addonsInfo in targeting Context"
    );
    Assert.equal(
      values.addonsInfo.hasInstalledAddons,
      true,
      "hasInstalledAddons is true"
    );
    Assert.deepEqual(
      values.addonsInfo.addons,
      [ext1.id, ext2.id].sort(),
      "Has two addons"
    );
  }, recordTargetingContext);

  await ext1.unload();
  await ext2.unload();

  await GleanPings.nimbusTargetingContext.testSubmission(() => {
    const values = getRecordedTargetingContextMetrics();

    Assert.ok(
      Object.hasOwn(values, "addonsInfo"),
      "addonsInfo in targeting Context"
    );
    Assert.equal(
      values.addonsInfo.hasInstalledAddons,
      false,
      "hasInstalledAddons is false"
    );
    Assert.deepEqual(
      values.addonsInfo.addons ?? [],
      [],
      "No recorded addon info"
    );
  }, recordTargetingContext);

  await cleanup();
});
