/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { PrefUtils } = ChromeUtils.importESModule(
  "resource://normandy/lib/PrefUtils.sys.mjs"
);
const { JsonSchema } = ChromeUtils.importESModule(
  "resource://gre/modules/JsonSchema.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const USER = "user";
const DEFAULT = "default";

const STRING_PREF = "test.nimbus.prefFlips.string";
const INT_PREF = "test.nimbus.prefFlips.int";
const BOOL_PREF = "test.nimbus.prefFlips.boolean";

const FEATURE_ID = "prefFlips";

const SET_BEFORE_VALUE = "set-before-value";
const USER_VALUE = "user-value";
const DEFAULT_VALUE = "default-value";

const PREF_FEATURES = {
  [USER]: new ExperimentFeature("test-set-pref-user-1", {
    description: "Test feature that sets prefs on the user branch via setPref",
    owner: "test@test.test",
    hasExposure: false,
    variables: {
      foo: {
        type: "string",
        description: "test variable",
        setPref: {
          branch: USER,
          pref: "nimbus.test-only.foo",
        },
      },
    },
  }),
  [DEFAULT]: new ExperimentFeature("test-set-pref-default-1", {
    description:
      "Test feature that sets prefs on the default branch via setPref",
    owner: "test@test.test",
    hasExposure: false,
    variables: {
      foo: {
        type: "string",
        description: "test variable",
        setPref: {
          branch: DEFAULT,
          pref: "nimbus.test-only.foo",
        },
      },
    },
  }),
};

function setPrefs(prefs) {
  for (const [name, { userBranchValue, defaultBranchValue }] of Object.entries(
    prefs
  )) {
    // If the different prefs have the same value, we must set the user branch
    // value first. Otherwise when we try to set the user branch value after
    // the default value, it will see the value already set for the user
    // branch (because it falls back to the default branch value) and will not
    // set it, leaving only a default branch pref.
    if (typeof userBranchValue !== "undefined") {
      PrefUtils.setPref(name, userBranchValue);
    }

    if (typeof defaultBranchValue !== "undefined") {
      PrefUtils.setPref(name, defaultBranchValue, { branch: DEFAULT });
    }
  }
}

function cleanupPrefs(prefs) {
  for (const name of Object.keys(prefs)) {
    Services.prefs.deleteBranch(name);
  }
}

function checkExpectedPrefs(prefs) {
  for (const [name, value] of Object.entries(prefs)) {
    Assert.equal(
      PrefUtils.getPref(name),
      value,
      `Pref ${name} has correct value`
    );
  }
}

function checkExpectedPrefBranches(prefs) {
  for (const [
    name,
    { defaultBranchValue = null, userBranchValue = null },
  ] of Object.entries(prefs)) {
    if (userBranchValue === null) {
      Assert.ok(
        !Services.prefs.prefHasUserValue(name),
        `Pref ${name} has no value on user branch`
      );
    } else {
      Assert.equal(
        PrefUtils.getPref(name, { branch: USER }),
        userBranchValue,
        `Pref ${name} has correct value on user branch`
      );
    }

    if (defaultBranchValue === null) {
      Assert.ok(
        !Services.prefs.prefHasDefaultValue(name),
        `Pref ${name} has no value on default branch`
      );
    } else {
      Assert.equal(
        PrefUtils.getPref(name, { branch: DEFAULT }),
        defaultBranchValue,
        `Pref ${name} has correct value on default branch`
      );
    }
  }
}

add_setup(function setup() {
  Services.fog.initializeFOG();
  Services.telemetry.clearEvents();

  registerCleanupFunction(
    NimbusTestUtils.addTestFeatures(PREF_FEATURES[USER], PREF_FEATURES[DEFAULT])
  );
});

async function setupTest({ ...args } = {}) {
  const { cleanup: baseCleanup, ...ctx } = await NimbusTestUtils.setupTest({
    ...args,
    clearTelemetry: true,
  });

  return {
    ...ctx,
    async cleanup() {
      assertNoObservers(ctx.manager);
      await baseCleanup();
    },
  };
}

add_task(async function test_schema() {
  const schema = await fetch(
    "resource://nimbus/schemas/PrefFlipsFeature.schema.json"
  ).then(rsp => rsp.json());
  const validator = new JsonSchema.Validator(schema);

  const ALLOWED_TEST_CASES = [
    { prefs: {} },
    {
      prefs: {
        "foo.string": {
          branch: USER,
          value: "value",
        },
        "foo.int": {
          branch: USER,
          value: 123,
        },
        "foo.bool": {
          branch: USER,
          value: true,
        },
        "bar.string": {
          branch: DEFAULT,
          value: "value",
        },
        "bar.int": {
          branch: DEFAULT,
          value: 345,
        },
        "bar.bool": {
          branch: DEFAULT,
          value: false,
        },
      },
    },
  ];

  for (const obj of ALLOWED_TEST_CASES) {
    const result = validator.validate(obj);
    Assert.ok(
      result.valid,
      `validated: ${JSON.stringify(result.errors, null, 2)}`
    );
  }

  const DISALLOWED_TEST_CASES = [
    {},
    {
      prefs: {
        "foo.bar.baz": {
          branch: "other",
          value: "value",
        },
      },
    },
    {
      prefs: {
        "foo.bar.baz": {},
      },
    },
    {
      prefs: {
        "foo.bar.baz": {
          branch: USER,
        },
      },
    },
    {
      prefs: {
        "foo.bar.baz": {
          branch: DEFAULT,
        },
      },
    },
    {
      prefs: {
        "foo.bar.baz": {
          value: "value",
        },
      },
    },
    {
      prefs: {
        "foo.bar.baz": {
          branch: DEFAULT,
          value: null,
        },
      },
    },
  ];

  for (const obj of DISALLOWED_TEST_CASES) {
    const result = validator.validate(obj);
    Assert.ok(!result.valid);
  }
});

add_task(async function test_prefFlips() {
  const setUserPrefs = {
    prefs: {
      [STRING_PREF]: {
        branch: USER,
        value: "hello, world",
      },
      [INT_PREF]: {
        branch: USER,
        value: 123,
      },
      [BOOL_PREF]: {
        branch: USER,
        value: true,
      },
    },
  };
  const setDefaultPrefs = {
    prefs: {
      [STRING_PREF]: {
        branch: DEFAULT,
        value: "hello, world",
      },
      [INT_PREF]: {
        branch: DEFAULT,
        value: 123,
      },
      [BOOL_PREF]: {
        branch: DEFAULT,
        value: true,
      },
    },
  };

  const clearUserPrefs = {
    prefs: {
      [STRING_PREF]: {
        branch: USER,
        value: null,
      },
      [INT_PREF]: {
        branch: USER,
        value: null,
      },
      [BOOL_PREF]: {
        branch: USER,
        value: null,
      },
    },
  };

  const PRE_SET_PREFS = {
    [USER]: {
      [STRING_PREF]: { userBranchValue: "goodbye, world" },
      [INT_PREF]: { userBranchValue: 234 },
      [BOOL_PREF]: { userBranchValue: false },
    },
    [DEFAULT]: {
      [STRING_PREF]: { defaultBranchValue: "goodbye, world" },
      [INT_PREF]: { defaultBranchValue: 234 },
      [BOOL_PREF]: { defaultBranchValue: false },
    },
    BOTH_BRANCHES: {
      [STRING_PREF]: {
        userBranchValue: USER_VALUE,
        defaultBranchValue: DEFAULT_VALUE,
      },
      [INT_PREF]: { userBranchValue: 2, defaultBranchValue: 3 },
      [BOOL_PREF]: { userBranchValue: false, defaultBranchValue: false },
    },
  };

  const TEST_CASES = [
    {
      name: "Set prefs on the user branch",
      featureValue: setUserPrefs,
    },
    {
      name: "Set prefs on the user branch with pre-existing values on the user branch",
      featureValue: setUserPrefs,
      setPrefsBefore: PRE_SET_PREFS[USER],
    },
    {
      name: "Set prefs on the user branch with pre-existing values on the default branch",
      featureValue: setUserPrefs,
      setPrefsBefore: PRE_SET_PREFS[DEFAULT],
    },
    {
      name: "Set prefs on the user branch with pre-existing values on both branches",
      featureValue: setUserPrefs,
      setPrefsBefore: PRE_SET_PREFS.BOTH_BRANCHES,
    },
    {
      name: "Set prefs on the default branch",
      featureValue: setDefaultPrefs,
    },
    {
      name: "Set prefs on the default branch with pre-existing values on the default branch",
      featureValue: setDefaultPrefs,
      setPrefsBefore: PRE_SET_PREFS[DEFAULT],
    },
    {
      name: "Set prefs on the default branch with pre-existing values on the user branch",
      featureValue: setDefaultPrefs,
      setPrefsBefore: PRE_SET_PREFS[USER],
      expectedPrefs: {
        [STRING_PREF]: PRE_SET_PREFS[USER][STRING_PREF].userBranchValue,
        [INT_PREF]: PRE_SET_PREFS[USER][INT_PREF].userBranchValue,
        [BOOL_PREF]: PRE_SET_PREFS[USER][BOOL_PREF].userBranchValue,
      },
    },
    {
      name: "Set prefs on the default branch with pre-existing values on both branches",
      featureValue: setDefaultPrefs,
      setPrefsBefore: PRE_SET_PREFS.BOTH_BRANCHES,
      expectedPrefs: {
        [STRING_PREF]: PRE_SET_PREFS.BOTH_BRANCHES[STRING_PREF].userBranchValue,
        [INT_PREF]: PRE_SET_PREFS.BOTH_BRANCHES[INT_PREF].userBranchValue,
        [BOOL_PREF]: PRE_SET_PREFS.BOTH_BRANCHES[BOOL_PREF].userBranchValue,
      },
    },
    {
      name: "Clearing prefs on the user branch (with value null) without pre-existing values",
      featureValue: clearUserPrefs,
    },
    {
      name: "Clearing prefs on the user branch (with value null) with pre-existing values on the user branch",
      featureValue: clearUserPrefs,
      setPrefsBefore: PRE_SET_PREFS[USER],
    },
    {
      name: "Clearing prefs on the user branch (with value null) with pre-existing values on the default branch",
      featureValue: clearUserPrefs,
      setPrefsBefore: PRE_SET_PREFS[DEFAULT],
      // This will not affect the default branch prefs.
      expectedPrefs: {
        [STRING_PREF]: PRE_SET_PREFS[DEFAULT][STRING_PREF].defaultBranchValue,
        [INT_PREF]: PRE_SET_PREFS[DEFAULT][INT_PREF].defaultBranchValue,
        [BOOL_PREF]: PRE_SET_PREFS[DEFAULT][BOOL_PREF].defaultBranchValue,
      },
    },
    {
      name: "Clearing prefs on the user branch (with value null) with pre-existing values on both branches",
      featureValue: clearUserPrefs,
      setPrefsBefore: PRE_SET_PREFS.BOTH_BRANCHES,
      expectedPrefs: {
        [STRING_PREF]:
          PRE_SET_PREFS.BOTH_BRANCHES[STRING_PREF].defaultBranchValue,
        [INT_PREF]: PRE_SET_PREFS.BOTH_BRANCHES[INT_PREF].defaultBranchValue,
        [BOOL_PREF]: PRE_SET_PREFS.BOTH_BRANCHES[BOOL_PREF].defaultBranchValue,
      },
    },
  ];

  for (const [i, { name, ...testCase }] of TEST_CASES.entries()) {
    info(`Running test case ${i}: ${name}`);

    const {
      // The feature config to enroll.
      featureValue,

      // Prefs that should be set before enrollment. These will be undone after
      // each test case.
      setPrefsBefore = {},

      // Additional prefs to check after enrollment. They will be checked on the
      // user branch.
      expectedPrefs = {},
    } = testCase;

    const { manager, cleanup } = await setupTest();

    info("Setting initial values of prefs...");
    setPrefs(setPrefsBefore);

    // Collect the values of any prefs that will be set by the enrollment so we
    // can compare their values after unenrollment.
    const prefValuesBeforeEnrollment = Object.fromEntries(
      Object.keys(featureValue.prefs).map(prefName => [
        prefName,
        PrefUtils.getPref(prefName),
      ])
    );

    info("Enrolling...");
    const cleanupExperiment = await NimbusTestUtils.enrollWithFeatureConfig(
      {
        featureId: FEATURE_ID,
        value: featureValue,
      },
      {
        manager,
        isRollout: true,
      }
    );

    info("Checking prefs were set by enrollment...");
    for (const [prefName, { branch, value }] of Object.entries(
      featureValue.prefs
    )) {
      if (typeof value === "undefined" || value === null) {
        if (branch === USER) {
          Assert.ok(
            !Services.prefs.prefHasUserValue(prefName),
            `${prefName} was cleared on the user branch`
          );
        } else if (prefValuesBeforeEnrollment[prefName] !== null) {
          // Can't clear the user branch.
          Assert.equal(
            PrefUtils.getPref(prefName, { branch }),
            prefValuesBeforeEnrollment
          );
        } else {
          Assert.equal(PrefUtils.getPref(prefName, { branch }), value);
        }
      } else {
        Assert.equal(PrefUtils.getPref(prefName, { branch }), value);
      }
    }

    if (expectedPrefs) {
      info("Checking expected prefs...");
      checkExpectedPrefs(expectedPrefs);
    }

    info("Unenrolling...");
    await cleanupExperiment();

    info("Checking prefs were restored after unenrollment...");
    // After unenrollment, the prefs should have been restored to their values
    // before enrollment.
    for (const [prefName, originalValue] of Object.entries(
      prefValuesBeforeEnrollment
    )) {
      // If the pref was set on the default branch, it won't be cleared. It will
      // persist until the next restart.
      const expectedValue =
        featureValue.prefs[prefName].branch === "default" &&
        originalValue === null
          ? featureValue.prefs[prefName].value
          : originalValue;
      Assert.equal(PrefUtils.getPref(prefName), expectedValue);
    }

    info("Cleaning up...");
    // Clear all the prefs we specified in `setPrefsBefore`.
    cleanupPrefs(setPrefsBefore);

    // Clear all prefs specified by the enrollment.
    for (const prefName of Object.keys(featureValue.prefs)) {
      Services.prefs.deleteBranch(prefName);
    }

    await cleanup();
  }
});

add_task(async function test_prefFlips_unenrollment() {
  const PREF_FOO = "nimbus.test-only.foo";
  const PREF_BAR = "nimbus.test-only.bar";

  const SLUG_1 = "slug-1";
  const SLUG_2 = "slug-2";
  const SLUG_3 = "slug-3";

  const EXPERIMENT_VALUE = "experiment-value";

  const TEST_CASES = [
    // Single enrollment case (experiments)
    {
      name: "set pref on the user branch with a prefFlips experiment and change that pref on the user branch",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER } },
          },
        },
      ],
      setPrefsAfter: { [PREF_FOO]: { userBranchValue: USER_VALUE } },
      expectedUnenrollments: [SLUG_1],
      expectedPrefs: { [PREF_FOO]: USER_VALUE },
    },
    {
      name: "set pref on the user branch with a prefFlips experiment and change that pref on the default branch",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER } },
          },
        },
      ],
      setPrefsAfter: { [PREF_FOO]: { defaultBranchValue: DEFAULT_VALUE } },
      expectedEnrollments: [SLUG_1],
      expectedPrefs: { [PREF_FOO]: EXPERIMENT_VALUE },
    },
    {
      name: "set pref on the default branch with a prefFlips experiment and change that pref on the user branch",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
      ],
      setPrefsAfter: { [PREF_FOO]: { userBranchValue: USER_VALUE } },
      expectedUnenrollments: [SLUG_1],
      expectedPrefs: { [PREF_FOO]: USER_VALUE },
    },
    {
      name: "set pref on the default branch with a prefFlips experiment and change that pref on the default branch",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
      ],
      setPrefsAfter: { [PREF_FOO]: { defaultBranchValue: DEFAULT_VALUE } },
      expectedUnenrollments: [SLUG_1],
      expectedPrefs: { [PREF_FOO]: DEFAULT_VALUE },
    },
    // Single enrollment case, multiple prefs being reset
    {
      name: "set prefs on the user branch with a prefFlips experiment and change one pref on the user branch",
      setPrefsBefore: { [PREF_FOO]: { userBranchValue: SET_BEFORE_VALUE } },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER },
              [PREF_BAR]: { value: EXPERIMENT_VALUE, branch: USER },
            },
          },
        },
      ],
      setPrefsAfter: { [PREF_BAR]: { userBranchValue: USER_VALUE } },
      expectedUnenrollments: [SLUG_1],
      expectedPrefs: { [PREF_FOO]: SET_BEFORE_VALUE, [PREF_BAR]: USER_VALUE },
    },
    {
      name: "set prefs on the user branch with a prefFlips experiment and change one pref on the default branch",
      setPrefsBefore: { [PREF_FOO]: { userBranchValue: SET_BEFORE_VALUE } },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER },
              [PREF_BAR]: { value: EXPERIMENT_VALUE, branch: USER },
            },
          },
        },
      ],
      setPrefsAfter: { [PREF_BAR]: { defaultBranchValue: DEFAULT_VALUE } },
      expectedEnrollments: [SLUG_1],
      expectedPrefs: {
        [PREF_FOO]: EXPERIMENT_VALUE,
        [PREF_BAR]: EXPERIMENT_VALUE,
      },
    },
    {
      name: "set prefs on the default branch with a prefFlips experiment and change one pref on the user branch",
      setPrefsBefore: { [PREF_FOO]: { defaultBranchValue: SET_BEFORE_VALUE } },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT },
              [PREF_BAR]: { value: EXPERIMENT_VALUE, branch: DEFAULT },
            },
          },
        },
      ],
      setPrefsAfter: { [PREF_BAR]: { userBranchValue: USER_VALUE } },
      expectedUnenrollments: [SLUG_1],
      expectedPrefs: { [PREF_FOO]: SET_BEFORE_VALUE, [PREF_BAR]: USER_VALUE },
    },
    {
      name: "set prefs on the default branch with a prefFlips experiment and change one pref on the default branch",
      setPrefsBefore: { [PREF_FOO]: { defaultBranchValue: SET_BEFORE_VALUE } },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT },
              [PREF_BAR]: { value: EXPERIMENT_VALUE, branch: DEFAULT },
            },
          },
        },
      ],
      setPrefsAfter: { [PREF_BAR]: { defaultBranchValue: DEFAULT_VALUE } },
      expectedUnenrollments: [SLUG_1],
      expectedPrefs: {
        [PREF_FOO]: SET_BEFORE_VALUE,
        [PREF_BAR]: DEFAULT_VALUE,
      },
    },
    // Multiple enrollment cases
    // - test that we leave enrollments that do not conflict with the set pref
    {
      name: "set pref on the user branch with two prefFlips experiments and then change a pref controlled by only one experiment on the user branch",
      setPrefsBefore: { [PREF_FOO]: { userBranchValue: SET_BEFORE_VALUE } },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER } },
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER },
              [PREF_BAR]: { value: EXPERIMENT_VALUE, branch: USER },
            },
          },
        },
      ],
      setPrefsAfter: { [PREF_BAR]: { userBranchValue: USER_VALUE } },
      expectedEnrollments: [SLUG_1],
      expectedUnenrollments: [SLUG_2],
      expectedPrefs: { [PREF_FOO]: EXPERIMENT_VALUE, [PREF_BAR]: USER_VALUE },
    },
    {
      name: "set pref on the user branch two prefFlips experiments and then change a pref controlled by only one experiment on the default branch",
      setPrefsBefore: { [PREF_FOO]: { userBranchValue: SET_BEFORE_VALUE } },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER } },
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER },
              [PREF_BAR]: { value: EXPERIMENT_VALUE, branch: USER },
            },
          },
        },
      ],
      setPrefsAfter: { [PREF_BAR]: { defaultBranchValue: DEFAULT_VALUE } },
      expectedEnrollments: [SLUG_1, SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: EXPERIMENT_VALUE,
        [PREF_BAR]: EXPERIMENT_VALUE,
      },
    },
    // - test that we unenroll from all conflicting experiments
    {
      name: "set pref on the default branch with two prefFlips experiments and then change that pref on the user branch",
      setPrefsBefore: { [PREF_FOO]: { defaultBranchValue: SET_BEFORE_VALUE } },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
      ],
      setPrefsAfter: { [PREF_FOO]: { userBranchValue: USER_VALUE } },
      expectedUnenrollments: [SLUG_1, SLUG_2],
      expectedPrefs: { [PREF_FOO]: USER_VALUE },
    },
    {
      name: "set pref on the default branch with two prefFlips experiments and then change that pref on the default branch",
      setPrefsBefore: { [PREF_FOO]: { defaultBranchValue: SET_BEFORE_VALUE } },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
      ],
      setPrefsAfter: { [PREF_FOO]: { defaultBranchValue: DEFAULT_VALUE } },
      expectedUnenrollments: [SLUG_1, SLUG_2],
      expectedPrefs: { [PREF_FOO]: DEFAULT_VALUE },
    },
    // - test we unenroll when the experiments conflict with eachother.
    {
      name: "set pref on the user branch with two experiments with conflicting values",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: SLUG_1, branch: USER } },
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: SLUG_2, branch: USER } },
          },
        },
      ],
      expectedEnrollments: [SLUG_1],
      expectedUnenrollments: [SLUG_2],
      expectedPrefs: { [PREF_FOO]: SLUG_1 },
    },
    {
      name: "set pref on the default branch with two experiments with conflicting values",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: SLUG_1, branch: DEFAULT } },
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: SLUG_2, branch: DEFAULT } },
          },
        },
      ],
      expectedEnrollments: [SLUG_1],
      expectedUnenrollments: [SLUG_2],
      expectedPrefs: { [PREF_FOO]: SLUG_1 },
    },
    {
      name: "set pref on the user branch with an experiment and set it on the default branch with another",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER } },
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
      ],
      expectedEnrollments: [SLUG_1],
      expectedUnenrollments: [SLUG_2],
      expectedPrefs: { [PREF_FOO]: EXPERIMENT_VALUE },
    },
    {
      name: "set pref on the default branch with an experiment and set it on the user branch with another",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER } },
          },
        },
      ],
      expectedEnrollments: [SLUG_1],
      expectedUnenrollments: [SLUG_2],
      expectedPrefs: { [PREF_FOO]: EXPERIMENT_VALUE },
    },

    // Multiple enrollment cases (prefFlips -> setPref)

    // NB: We don't need to test setPref experiments/rollouts on both branches
    // for the same pref because that configuration is prohibited by
    // gen_feature_manifests.py

    // * prefFlip experiments -> setPref experiment
    {
      name: "enroll in prefFlips experiments on the user branch and then a setPref experiment on the user branch",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER } },
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER } },
          },
        },
        {
          slug: SLUG_3,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
      ],
      expectedEnrollments: [SLUG_3],
      expectedUnenrollments: [SLUG_1, SLUG_2],
      expectedPrefs: { [PREF_FOO]: EXPERIMENT_VALUE },
    },
    {
      name: "enroll in prefFlips experiments on the default branch and then a setPref experiment on the user branch",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
        {
          slug: SLUG_3,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
      ],
      expectedEnrollments: [SLUG_3],
      expectedUnenrollments: [SLUG_1, SLUG_2],
      expectedPrefs: { [PREF_FOO]: EXPERIMENT_VALUE },
    },
    {
      name: "enroll in prefFlips experiments on the user branch and then a setPref experiment on the default branch",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER } },
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER } },
          },
        },
        {
          slug: SLUG_3,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
      ],
      expectedEnrollments: [SLUG_3],
      expectedUnenrollments: [SLUG_1, SLUG_2],
      expectedPrefs: { [PREF_FOO]: EXPERIMENT_VALUE },
    },
    {
      name: "enroll in prefFlips experiments on the default branch and then a setPref experiment on the default branch",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
        {
          slug: SLUG_3,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
      ],
      expectedEnrollments: [SLUG_3],
      expectedUnenrollments: [SLUG_1, SLUG_2],
      expectedPrefs: { [PREF_FOO]: EXPERIMENT_VALUE },
    },
    {
      name: "enroll in prefFlip experiment on the user branch and then a setPref experiment on the user branch and unenroll to check if original values are restored (no original value)",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: SLUG_1, branch: USER },
            },
          },
        },
        {
          slug: SLUG_2,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: SLUG_2,
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: null,
      },
    },
    {
      name: "enroll in prefFlip experiment on the user branch and then a setPref experiment on the default branch and unenroll to check if original values are restored (no original value)",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: SLUG_1, branch: USER },
            },
          },
        },
        {
          slug: SLUG_2,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: SLUG_2,
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SLUG_2, // we can't clear the default branch
      },
    },
    {
      name: "enroll in prefFlip experiment on the default branch and then a setPref experiment on the user branch and unenroll to check if original values are restored (no original value)",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: SLUG_1, branch: USER },
            },
          },
        },
        {
          slug: SLUG_2,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: SLUG_2,
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: null,
      },
    },
    {
      name: "enroll in prefFlip experiment on the default branch and then a setPref experiment on the default branch and unenroll to check if original values are restored (no original value)",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: SLUG_1, branch: USER },
            },
          },
        },
        {
          slug: SLUG_2,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: SLUG_2,
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SLUG_2,
      },
    },
    {
      name: "enroll in prefFlip experiment on the user branch and then a setPref experiment on the user branch and unenroll to check if original values are restored",
      setPrefsBefore: { [PREF_FOO]: { userBranchValue: SET_BEFORE_VALUE } },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: SLUG_1, branch: USER },
            },
          },
        },
        {
          slug: SLUG_2,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: SLUG_2,
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SET_BEFORE_VALUE,
      },
    },
    {
      name: "enroll in prefFlip experiment on the user branch and then a setPref experiment on the default branch and unenroll to check if original values are restored",
      setPrefsBefore: { [PREF_FOO]: { defaultBranchValue: SET_BEFORE_VALUE } },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: SLUG_1, branch: USER },
            },
          },
        },
        {
          slug: SLUG_2,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: SLUG_2,
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SET_BEFORE_VALUE,
      },
    },
    {
      name: "enroll in prefFlip experiment on the default branch and then a setPref experiment on the user branch and unenroll to check if original values are restored",
      setPrefsBefore: { [PREF_FOO]: { defaultBranchValue: SET_BEFORE_VALUE } },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: SLUG_1, branch: USER },
            },
          },
        },
        {
          slug: SLUG_2,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: SLUG_2,
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SET_BEFORE_VALUE,
      },
    },
    {
      name: "enroll in prefFlip experiment on the default branch and then a setPref experiment on the default branch and unenroll to check if original values are restored",
      setPrefsBefore: { [PREF_FOO]: { defaultBranchValue: SET_BEFORE_VALUE } },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: SLUG_1, branch: USER },
            },
          },
        },
        {
          slug: SLUG_2,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: SLUG_2,
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SET_BEFORE_VALUE,
      },
    },
    // * setPref experiment, rollout -> prefFlips experiment
    {
      name: "enroll in a setPref experiment and rollout on the user branch then a prefFlips experiment on the user branch",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
        {
          slug: SLUG_2,
          isRollout: true,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
        {
          slug: SLUG_3,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER } },
          },
        },
      ],
      expectedEnrollments: [SLUG_3],
      expectedUnenrollments: [SLUG_1, SLUG_2],
      expectedPrefs: { [PREF_FOO]: EXPERIMENT_VALUE },
    },
    {
      name: "enroll in a setPref experiment and rollout on the user branch then a prefFlips experiment on the default branch",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
        {
          slug: SLUG_2,
          isRollout: true,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
        {
          slug: SLUG_3,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
      ],
      expectedEnrollments: [SLUG_3],
      expectedUnenrollments: [SLUG_1, SLUG_2],
      expectedPrefs: { [PREF_FOO]: EXPERIMENT_VALUE },
    },
    {
      name: "enroll in a setPref experiment and rollout on the default branch then a prefFlips experiment on the user branch",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
        {
          slug: SLUG_2,
          isRollout: true,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
        {
          slug: SLUG_3,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER } },
          },
        },
      ],
      expectedEnrollments: [SLUG_3],
      expectedUnenrollments: [SLUG_1, SLUG_2],
      expectedPrefs: { [PREF_FOO]: EXPERIMENT_VALUE },
    },
    {
      name: "enroll in a setPref experiment and rollout on the default branch then a prefFlips experiment on the default branch",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
        {
          slug: SLUG_2,
          isRollout: true,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
        {
          slug: SLUG_3,
          featureId: FEATURE_ID,
          value: {
            prefs: { [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT } },
          },
        },
      ],
      expectedEnrollments: [SLUG_3],
      expectedUnenrollments: [SLUG_1, SLUG_2],
      expectedPrefs: { [PREF_FOO]: EXPERIMENT_VALUE },
    },
    {
      name: "enroll in a setPref experiment on the user branch and a prefFlips experiment on the user branch and unenroll to check if original values are restored (no original value)",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: SLUG_1,
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: SLUG_2, branch: USER },
            },
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: null, // we can't clear the default branch
      },
    },
    {
      name: "enroll in a setPref experiment on the user branch and a prefFlips experiment on the default branch and unenroll to check if original values are restored (no original value)",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: SLUG_1,
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: SLUG_2, branch: DEFAULT },
            },
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SLUG_2, // Cannot clear the default branch
      },
    },
    {
      name: "enroll in a setPref experiment on the default branch and a prefFlips experiment on the user branch and unenroll to check if original values are restored (no original value)",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: SLUG_1,
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: SLUG_2, branch: USER },
            },
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SLUG_1, // cannot clear the default branch
      },
    },
    {
      name: "enroll in a setPref experiment on the default branch and a prefFlips experiment on the default branch and unenroll to check if original values are restored (no original value)",
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: SLUG_1,
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: SLUG_2, branch: DEFAULT },
            },
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SLUG_2, // cannot clear the default branch
      },
    },
    {
      name: "enroll in a setPref experiment on the user branch and a prefFlips experiment on the user branch and unenroll to check if original values are restored",
      setPrefsBefore: {
        [PREF_FOO]: { userBranchValue: SET_BEFORE_VALUE },
      },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER },
            },
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SET_BEFORE_VALUE,
      },
    },
    {
      name: "enroll in a setPref experiment on the user branch and a prefFlips experiment on the default branch and unenroll to check if original values are restored",
      setPrefsBefore: {
        [PREF_FOO]: { userBranchValue: SET_BEFORE_VALUE },
      },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: PREF_FEATURES[USER].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT },
            },
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SET_BEFORE_VALUE,
      },
    },
    {
      name: "enroll in a setPref experiment on the default branch and a prefFlips experiment on the user branch and unenroll to check if original values are restored",
      setPrefsBefore: {
        [PREF_FOO]: { defaultBranchValue: SET_BEFORE_VALUE },
      },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: USER },
            },
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SET_BEFORE_VALUE,
      },
    },
    {
      name: "enroll in a setPref experiment on the default branch and a prefFlips experiment on the default branch and unenroll to check if original values are restored",
      setPrefsBefore: {
        [PREF_FOO]: { defaultBranchValue: SET_BEFORE_VALUE },
      },
      enrollmentOrder: [
        {
          slug: SLUG_1,
          featureId: PREF_FEATURES[DEFAULT].featureId,
          value: {
            foo: EXPERIMENT_VALUE,
          },
        },
        {
          slug: SLUG_2,
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_FOO]: { value: EXPERIMENT_VALUE, branch: DEFAULT },
            },
          },
        },
      ],
      expectedEnrollments: [SLUG_2],
      expectedUnenrollments: [SLUG_1],
      unenrollmentOrder: [SLUG_2],
      expectedPrefs: {
        [PREF_FOO]: SET_BEFORE_VALUE,
      },
    },
  ];

  for (const [i, { name, ...testCase }] of TEST_CASES.entries()) {
    info(`Running test case ${i}: ${name}`);

    const {
      // Prefs that should be set after enrollment. These will be undone after
      // each test case.
      setPrefsBefore = {},
      // The slugs to enroll in the order they should be enrolled in, and
      // whether or not they should enroll as rollouts.
      enrollmentOrder,
      // Prefs that should be set after enrollment. These will be undone
      // after each test case.
      setPrefsAfter = {},
      // The expected active enrollments after all enrollments have finished.
      expectedEnrollments = [],
      // The expected inactive enrollments after all enrollments have finished.
      expectedUnenrollments = [],
      // The slugs to unenroll from after enrolling and settings prefs but
      // before checking pref values.
      unenrollmentOrder,
      // Prefs to check after enrollment. They will be checked on the user
      // branch.
      expectedPrefs,
    } = testCase;

    info("Setting prefs before enrollment...");
    setPrefs(setPrefsBefore);

    const { manager, cleanup } = await setupTest();

    info("Enrolling...");
    for (const {
      slug,
      isRollout = false,
      ...featureConfig
    } of enrollmentOrder) {
      await NimbusTestUtils.enrollWithFeatureConfig(featureConfig, {
        slug,
        manager,
        isRollout,
      });
    }

    info("Setting prefs after enrollment...");
    setPrefs(setPrefsAfter);

    info("Checking expected enrollments...");
    for (const slug of expectedEnrollments) {
      const enrollment = manager.store.get(slug);

      Assert.ok(
        enrollment !== null && typeof enrollment !== "undefined",
        `An enrollment for ${slug} should exist`
      );
      Assert.ok(enrollment.active, `It should still be active`);
    }

    info("Checking expected unenrollments...");
    for (const slug of expectedUnenrollments) {
      const enrollment = manager.store.get(slug);

      Assert.ok(!!enrollment, `An enrollment for ${slug} should exist`);
      Assert.ok(!enrollment.active, "It should no longer be active");
    }

    if (unenrollmentOrder) {
      info("Unenrolling from specific experiments before checking prefs...");
      for (const slug of unenrollmentOrder ?? []) {
        await manager.unenroll(slug);
      }
    }

    if (expectedPrefs) {
      info("Checking expected prefs...");
      checkExpectedPrefs(expectedPrefs);
    }

    info("Unenrolling from active experiments...");
    for (const slug of expectedEnrollments) {
      if (!(unenrollmentOrder ?? []).includes(slug)) {
        info(`Unenrolling from ${slug}\n`);
        await manager.unenroll(slug);
      }
    }

    info("Cleaning up prefs...");
    Services.prefs.deleteBranch(PREF_FOO);
    Services.prefs.deleteBranch(PREF_BAR);

    await cleanup();
  }
});

add_task(async function test_prefFlip_setPref_restore() {
  const PREF = "nimbus.test-only.foo";

  const SET_PREF_USER = "set-pref-user";
  const SET_PREF_DEFAULT = "set-pref-default";
  const PREF_FLIPS_USER = "pref-flips-user";
  const PREF_FLIPS_DEFAULT = "pref-flips-default";

  const FEATURE_CONFIGS = {
    [SET_PREF_USER]: {
      featureId: PREF_FEATURES[USER].featureId,
      value: {
        foo: SET_PREF_USER,
      },
    },
    [SET_PREF_DEFAULT]: {
      featureId: PREF_FEATURES[DEFAULT].featureId,
      value: {
        foo: SET_PREF_DEFAULT,
      },
    },
    [PREF_FLIPS_USER]: {
      featureId: FEATURE_ID,
      value: {
        prefs: {
          [PREF]: {
            branch: USER,
            value: PREF_FLIPS_USER,
          },
        },
      },
    },
    [PREF_FLIPS_DEFAULT]: {
      featureId: FEATURE_ID,
      value: {
        prefs: {
          [PREF]: {
            branch: DEFAULT,
            value: PREF_FLIPS_DEFAULT,
          },
        },
      },
    },
  };

  const TEST_CASES = [
    // 1. No prefs set beforehand.
    // - setPref first
    {
      name: "enroll in setPref on user branch and prefFlips on user branch",
      enrollmentOrder: [SET_PREF_USER, PREF_FLIPS_USER],
      expectedPrefs: { [PREF]: {} },
    },
    {
      name: "enroll in setPref on user branch and prefFlips on default branch",
      enrollmentOrder: [SET_PREF_USER, PREF_FLIPS_DEFAULT],
      expectedPrefs: { [PREF]: { defaultBranchValue: PREF_FLIPS_DEFAULT } },
    },
    {
      name: "enroll in setPref on default branch and prefFlips on user branch",
      enrollmentOrder: [SET_PREF_DEFAULT, PREF_FLIPS_USER],
      expectedPrefs: { [PREF]: { defaultBranchValue: SET_PREF_DEFAULT } },
    },
    {
      name: "enroll in setPref on default branch and prefFlips on default branch",
      enrollmentOrder: [SET_PREF_DEFAULT, PREF_FLIPS_DEFAULT],
      // We can't clear the default branch
      expectedPrefs: { [PREF]: { defaultBranchValue: PREF_FLIPS_DEFAULT } },
    },
    // - prefFlips first
    {
      name: "enroll in prefFlips on user branch and setPref on user branch",
      enrollmentOrder: [PREF_FLIPS_USER, SET_PREF_USER],
      expectedPrefs: { [PREF]: {} },
    },
    {
      name: "enroll in prefFlips on user branch and setPref on default branch",
      enrollmentOrder: [PREF_FLIPS_USER, SET_PREF_DEFAULT],
      expectedPrefs: { [PREF]: { defaultBranchValue: SET_PREF_DEFAULT } },
    },
    {
      name: "enroll in prefFlips on default branch and setPref on user branch",
      enrollmentOrder: [PREF_FLIPS_DEFAULT, SET_PREF_USER],
      expectedPrefs: { [PREF]: { defaultBranchValue: PREF_FLIPS_DEFAULT } },
    },
    {
      name: "enroll in prefFlips on default branch and setPref on default branch",
      enrollmentOrder: [PREF_FLIPS_DEFAULT, SET_PREF_DEFAULT],
      // We can't clear the default branch
      expectedPrefs: { [PREF]: { defaultBranchValue: SET_PREF_DEFAULT } },
    },
    // 2. User branch prefs set beforehand.
    // - setPref first
    {
      name: "set prefs on user branch and enroll in setPref on user branch and prefFlips on user branch",
      setPrefsBefore: { [PREF]: { userBranchValue: USER_VALUE } },
      enrollmentOrder: [SET_PREF_USER, PREF_FLIPS_USER],
      expectedPrefs: { [PREF]: { userBranchValue: USER_VALUE } },
    },
    {
      name: "set prefs on user branch and enroll in setPref on user branch and prefFlips on default branch",
      setPrefsBefore: { [PREF]: { userBranchValue: USER_VALUE } },
      enrollmentOrder: [SET_PREF_USER, PREF_FLIPS_DEFAULT],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: PREF_FLIPS_DEFAULT,
        },
      },
    },
    {
      name: "set prefs on user branch and enroll in setPref on default branch and prefFlips on user branch",
      setPrefsBefore: { [PREF]: { userBranchValue: USER_VALUE } },
      enrollmentOrder: [SET_PREF_DEFAULT, PREF_FLIPS_USER],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: SET_PREF_DEFAULT,
        },
      },
    },
    {
      name: "set prefs on user branch and enroll in setPref on default branch and prefFlips on default branch",
      setPrefsBefore: { [PREF]: { userBranchValue: USER_VALUE } },
      enrollmentOrder: [SET_PREF_DEFAULT, PREF_FLIPS_DEFAULT],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: PREF_FLIPS_DEFAULT, // We can't clear the default branch
        },
      },
    },
    // - prefFlips first
    {
      name: "set prefs on user branch and enroll in prefFlips on user branch and setPref on user branch",
      setPrefsBefore: { [PREF]: { userBranchValue: USER_VALUE } },
      enrollmentOrder: [PREF_FLIPS_USER, SET_PREF_USER],
      expectedPrefs: { [PREF]: { userBranchValue: USER_VALUE } },
    },
    {
      name: "set prefs on user branch and enroll in prefFlips on user branch and setPref on default branch",
      setPrefsBefore: { [PREF]: { userBranchValue: USER_VALUE } },
      enrollmentOrder: [PREF_FLIPS_USER, SET_PREF_DEFAULT],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: SET_PREF_DEFAULT,
        },
      },
    },
    {
      name: "set prefs on user branch and enroll in prefFlips on default branch and setPref on user branch",
      setPrefsBefore: { [PREF]: { userBranchValue: USER_VALUE } },
      enrollmentOrder: [PREF_FLIPS_DEFAULT, SET_PREF_USER],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: PREF_FLIPS_DEFAULT,
        },
      },
    },
    {
      name: "set prefs on user branch and enroll in prefFlips on default branch and setPref on default branch",
      setPrefsBefore: { [PREF]: { userBranchValue: USER_VALUE } },
      enrollmentOrder: [PREF_FLIPS_DEFAULT, SET_PREF_DEFAULT],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: SET_PREF_DEFAULT, // We can't clear the default branch
        },
      },
    },
    // 3. Default branch prefs set beforehand.
    // - setPref first
    {
      setPrefsBefore: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
      name: "set prefs on default branch and enroll branch in setPref on user branch and prefFlips on user branch",
      enrollmentOrder: [SET_PREF_USER, PREF_FLIPS_USER],
      expectedPrefs: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
    },
    {
      setPrefsBefore: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
      name: "set prefs on default branch and enroll branch in setPref on user branch and prefFlips on default branch",
      enrollmentOrder: [SET_PREF_USER, PREF_FLIPS_DEFAULT],
      expectedPrefs: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
    },
    {
      setPrefsBefore: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
      name: "set prefs on default branch and enroll branch in setPref on default branch and prefFlips on user branch",
      enrollmentOrder: [SET_PREF_DEFAULT, PREF_FLIPS_USER],
      expectedPrefs: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
    },
    {
      setPrefsBefore: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
      name: "set prefs on default branch and enroll branch in setPref on default branch and prefFlips on default branch",
      enrollmentOrder: [SET_PREF_DEFAULT, PREF_FLIPS_DEFAULT],
      expectedPrefs: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
    },
    // - prefFlips first
    {
      setPrefsBefore: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
      name: "set prefs on default branch and enroll branch in prefFlips on user branch and setPref on user branch",
      enrollmentOrder: [PREF_FLIPS_USER, SET_PREF_USER],
      expectedPrefs: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
    },
    {
      name: "set prefs on default branch and enroll branch in prefFlips on user branch and setPref on default branch",
      setPrefsBefore: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
      enrollmentOrder: [PREF_FLIPS_USER, SET_PREF_DEFAULT],
      expectedPrefs: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
    },
    {
      name: "set prefs on default branch and enroll branch in prefFlips on default branch and setPref on user branch",
      setPrefsBefore: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
      enrollmentOrder: [PREF_FLIPS_DEFAULT, SET_PREF_USER],
      expectedPrefs: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
    },
    {
      name: "set prefs on default branch and enroll branch in prefFlips on default branch and setPref on default branch",
      setPrefsBefore: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
      enrollmentOrder: [PREF_FLIPS_DEFAULT, SET_PREF_DEFAULT],
      expectedPrefs: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
    },
    {
      name: "set prefs on default branch and enroll branch in prefFlips on default branch and setPref on default branch, unenrolling in reverse order",
      setPrefsBefore: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
      enrollmentOrder: [PREF_FLIPS_DEFAULT, SET_PREF_DEFAULT],
      unenrollInReverseOrder: true,
      expectedPrefs: { [PREF]: { defaultBranchValue: DEFAULT_VALUE } },
    },
    // 4. Both user and default branch prefs set beforehand.
    // - setPref first
    {
      setPrefsBefore: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
      name: "set prefs on both branches and enroll branch in setPref on user branch and prefFlips on user branch",
      enrollmentOrder: [SET_PREF_USER, PREF_FLIPS_USER],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
    },
    {
      setPrefsBefore: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
      name: "set prefs on both branches and enroll branch in setPref on user branch and prefFlips on default branch",
      enrollmentOrder: [SET_PREF_USER, PREF_FLIPS_DEFAULT],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
    },
    {
      setPrefsBefore: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
      name: "set prefs on both branches and enroll branch in setPref on default branch and prefFlips on user branch",
      enrollmentOrder: [SET_PREF_DEFAULT, PREF_FLIPS_USER],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
    },
    {
      setPrefsBefore: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
      name: "set prefs on both branches and enroll branch in setPref on default branch and prefFlips on default branch",
      enrollmentOrder: [SET_PREF_DEFAULT, PREF_FLIPS_DEFAULT],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
    },
    // - prefFlips first
    {
      setPrefsBefore: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
      name: "set prefs on both branches and enroll branch in prefFlips on user branch and setPref on user branch",
      enrollmentOrder: [PREF_FLIPS_USER, SET_PREF_USER],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
    },
    {
      name: "set prefs on both branches and enroll branch in prefFlips on user branch and setPref on default branch",
      setPrefsBefore: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
      enrollmentOrder: [PREF_FLIPS_USER, SET_PREF_DEFAULT],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
    },
    {
      name: "set prefs on both branches and enroll branch in prefFlips on default branch and setPref on user branch",
      setPrefsBefore: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
      enrollmentOrder: [PREF_FLIPS_DEFAULT, SET_PREF_USER],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
    },
    {
      name: "set prefs on both branches and enroll branch in prefFlips on default branch and setPref on default branch",
      setPrefsBefore: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
      enrollmentOrder: [PREF_FLIPS_DEFAULT, SET_PREF_DEFAULT],
      expectedPrefs: {
        [PREF]: {
          userBranchValue: USER_VALUE,
          defaultBranchValue: DEFAULT_VALUE,
        },
      },
    },
  ];

  for (const [i, { name, ...testCase }] of TEST_CASES.entries()) {
    info(`Running test case ${i}: ${name}`);

    const { setPrefsBefore = {}, enrollmentOrder, expectedPrefs } = testCase;

    info("Setting prefs before enrollment...");
    setPrefs(setPrefsBefore);

    const { manager, cleanup } = await setupTest();

    info("Enrolling...");
    for (const slug of enrollmentOrder) {
      await NimbusTestUtils.enrollWithFeatureConfig(FEATURE_CONFIGS[slug], {
        manager,
        slug,
      });
    }

    info("Checking expected enrollments...");
    {
      const enrollment = manager.store.get(enrollmentOrder[0]);
      Assert.ok(
        enrollment !== null,
        `An enrollment for ${enrollmentOrder[0]} should exist`
      );
      Assert.ok(!enrollment.active, "It should no longer be active.");
    }
    {
      const enrollment = manager.store.get(enrollmentOrder[1]);
      Assert.ok(
        enrollment !== null,
        `An enrollment for ${enrollmentOrder[1]} should exist`
      );
      Assert.ok(enrollment.active, "It should be active.");
    }

    info("Checking submitted telemetry...");
    TelemetryTestUtils.assertEvents(
      [
        {
          value: enrollmentOrder[0],
          extra: {
            reason: "prefFlips-conflict",
            conflictingSlug: enrollmentOrder[1],
          },
        },
      ],
      {
        category: "normandy",
        object: "nimbus_experiment",
        method: "unenroll",
      }
    );
    Assert.deepEqual(
      Glean.nimbusEvents.unenrollment.testGetValue("events").map(event => ({
        reason: event.extra.reason,
        experiment: event.extra.experiment,
        conflicting_slug: event.extra.conflicting_slug,
      })),
      [
        {
          reason: "prefFlips-conflict",
          experiment: enrollmentOrder[0],
          conflicting_slug: enrollmentOrder[1],
        },
      ]
    );
    Assert.deepEqual(
      Glean.nimbusEvents.enrollmentStatus
        .testGetValue("events")
        ?.map(ev => ev.extra),
      [
        {
          slug: enrollmentOrder[0],
          branch: "control",
          status: "Enrolled",
          reason: "Qualified",
        },
        {
          slug: enrollmentOrder[0],
          branch: "control",
          status: "Disqualified",
          reason: "PrefFlipsConflict",
          conflict_slug: enrollmentOrder[1],
        },
        {
          slug: enrollmentOrder[1],
          branch: "control",
          status: "Enrolled",
          reason: "Qualified",
        },
      ]
    );

    info("Unenrolling...");
    await manager.unenroll(enrollmentOrder[1]);

    info("Checking expected prefs...");
    checkExpectedPrefBranches(expectedPrefs);

    await cleanup();

    info("Cleaning up prefs...");
    Services.prefs.deleteBranch(PREF);
  }
});

add_task(async function test_prefFlips_cacheOriginalValues() {
  const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
    "prefFlips-test",
    {
      featureId: FEATURE_ID,
      value: {
        prefs: {
          "test.pref.please.ignore": {
            branch: "user",
            value: "test-value",
          },
        },
      },
    }
  );

  const { manager, cleanup } = await setupTest();

  await manager.enroll(recipe, "test");

  const activeEnrollment = manager.store.getExperimentForFeature(FEATURE_ID);

  Assert.deepEqual(activeEnrollment.prefFlips, {
    originalValues: {
      "test.pref.please.ignore": null,
    },
  });

  const storePath = manager.store._store.path;

  // We are intentionally *not* forcing a save -- we are only flushing a pending
  // save to disk.
  {
    const jsonFile = manager.store._store;
    if (jsonFile._saver.isRunning) {
      await jsonFile._saver._runningPromise;
    } else if (jsonFile._saver.isArmed) {
      jsonFile._saver.disarm();
      await jsonFile._save();
    }
  }

  const storeContents = await IOUtils.readJSON(storePath);

  Assert.ok(
    Object.hasOwn(storeContents, "prefFlips-test"),
    "enrollment present in serialized store"
  );
  Assert.ok(
    Object.hasOwn(storeContents["prefFlips-test"], "prefFlips"),
    "prefFlips cache preset in serialized enrollment"
  );

  Assert.deepEqual(
    storeContents["prefFlips-test"].prefFlips,
    {
      originalValues: {
        "test.pref.please.ignore": null,
      },
    },
    "originalValues cached on serialized enrollment"
  );

  await manager.unenroll(recipe.slug);
  Assert.ok(
    !Services.prefs.prefHasUserValue("test.pref.please.ignore"),
    "pref unset after unenrollment"
  );

  await cleanup();
});

add_task(async function test_prefFlips_restore_unenroll() {
  const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
    "prefFlips-test",
    {
      featureId: FEATURE_ID,
      value: {
        prefs: {
          "test.pref.please.ignore": {
            branch: "user",
            value: "test-value",
          },
        },
      },
    }
  );

  // Set up a previous ExperimentStore on disk.
  let storePath;
  {
    const enrollment = {
      slug: recipe.slug,
      branch: recipe.branches[0],
      active: true,
      experimentType: "nimbus",
      userFacingName: recipe.userFacingName,
      userFacingDescription: recipe.userFacingDescription,
      featureIds: recipe.featureIds,
      isRollout: recipe.isRollout,
      localizations: recipe.localizations,
      source: "rs-loader",
      prefFlips: {
        originalValues: {
          "test.pref.please.ignore": null,
        },
      },
      lastSeen: new Date().toJSON(),
    };

    const store = NimbusTestUtils.stubs.store();
    await store.init();
    store.set(enrollment.slug, enrollment);
    storePath = await NimbusTestUtils.saveStore(store);
  }

  // Set the pref controlled by the experiment.
  Services.prefs.setStringPref("test.pref.please.ignore", "test-value");

  const { manager, cleanup } = await setupTest({
    storePath,
    secureExperiments: [recipe],
  });

  const activeEnrollment = manager.store.getExperimentForFeature(FEATURE_ID);
  Assert.equal(activeEnrollment.slug, recipe.slug, "enrollment restored");

  Assert.equal(
    manager._prefFlips._getOriginalValue("test.pref.please.ignore", "user"),
    null
  );

  await manager.unenroll(recipe.slug);
  Assert.ok(
    !Services.prefs.prefHasUserValue("test.pref.please.ignore"),
    "pref unset after unenrollment"
  );

  await cleanup();
});

add_task(async function test_prefFlips_failed() {
  const PREF = "test.pref.please.ignore";

  Services.prefs.getDefaultBranch(null).setStringPref(PREF, "test-value");

  const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
    "prefFlips-test",
    {
      featureId: FEATURE_ID,
      value: {
        prefs: {
          [PREF]: { branch: "user", value: 123 },
        },
      },
    }
  );

  const { manager, cleanup } = await setupTest();
  await manager.enroll(recipe, "test");

  const enrollment = manager.store.get(recipe.slug);
  Assert.ok(!enrollment.active, "Experiment should not be active");

  Assert.equal(Services.prefs.getStringPref(PREF), "test-value");

  TelemetryTestUtils.assertEvents(
    [
      {
        value: recipe.slug,
        extra: {
          reason: "prefFlips-failed",
          prefName: PREF,
          prefType: "string",
        },
      },
    ],
    {
      category: "normandy",
      object: "nimbus_experiment",
      method: "unenroll",
    }
  );
  Assert.deepEqual(
    Glean.nimbusEvents.unenrollment.testGetValue("events").map(event => ({
      reason: event.extra.reason,
      experiment: event.extra.experiment,
      pref_name: event.extra.pref_name,
      pref_type: event.extra.pref_type,
    })),
    [
      {
        reason: "prefFlips-failed",
        experiment: recipe.slug,
        pref_name: PREF,
        pref_type: "string",
      },
    ]
  );

  Services.prefs.deleteBranch(PREF);

  await cleanup();
});

add_task(async function test_prefFlips_failed_multiple_prefs() {
  const GOOD_PREF = "test.pref.please.ignore";
  const BAD_PREF = "this.one.too";

  Services.prefs.getDefaultBranch(null).setStringPref(BAD_PREF, "test-value");

  const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
    "prefFlips-test",
    {
      featureId: FEATURE_ID,
      value: {
        prefs: {
          [GOOD_PREF]: { branch: USER, value: 123 },
          [BAD_PREF]: { branch: USER, value: 123 },
        },
      },
    }
  );

  const { sandbox, manager, cleanup } = await setupTest();

  const setPrefSpy = sandbox.spy(PrefUtils, "setPref");

  await manager.enroll(recipe, "test");

  const enrollment = manager.store.get(recipe.slug);
  Assert.ok(!enrollment.active, "Experiment should not be active");

  Assert.deepEqual(
    setPrefSpy.getCall(0).args,
    [GOOD_PREF, 123, { branch: USER }],
    `should set ${GOOD_PREF}`
  );
  Assert.deepEqual(
    setPrefSpy.getCall(1).args,
    [BAD_PREF, 123, { branch: USER }],
    `should have attempted to set ${BAD_PREF}`
  );
  Assert.ok(
    typeof setPrefSpy.getCall(1).exception !== "undefined",
    `Attempting to set ${BAD_PREF} threw`
  );
  Assert.deepEqual(
    setPrefSpy.getCall(2).args,
    [GOOD_PREF, null, { branch: USER }],
    `should reset ${GOOD_PREF}`
  );
  Assert.equal(
    setPrefSpy.callCount,
    3,
    "should have 3 calls to PrefUtils.setPref"
  );

  Assert.ok(
    !Services.prefs.prefHasUserValue(GOOD_PREF),
    `${GOOD_PREF} should not be set`
  );
  Assert.equal(Services.prefs.getStringPref(BAD_PREF), "test-value");

  Services.prefs.deleteBranch(GOOD_PREF);
  Services.prefs.deleteBranch(BAD_PREF);

  await cleanup();
});

add_task(async function test_prefFlips_failed_experiment_and_rollout_1() {
  const ROLLOUT = "rollout";
  const EXPERIMENT = "experiment";

  const PREFS = {
    [ROLLOUT]: "test.nimbus.prefs.rollout-1",
    [EXPERIMENT]: "test.nimbus.prefs.experiment-1",
  };

  const VALUES = {
    [ROLLOUT]: "rollout-value",
    [EXPERIMENT]: "experiment-value",
  };

  const BOGUS_VALUE = 123;

  const TEST_CASES = [
    {
      name: "Enrolling in an experiment and then a rollout with errors",
      setPrefsBefore: {
        [PREFS[ROLLOUT]]: { defaultBranchValue: BOGUS_VALUE },
      },
      enrollmentOrder: [EXPERIMENT, ROLLOUT],
      expectedEnrollments: [EXPERIMENT],
      expectedUnenrollments: [ROLLOUT],
      expectedPrefs: {
        [PREFS[EXPERIMENT]]: VALUES[EXPERIMENT],
        [PREFS[ROLLOUT]]: BOGUS_VALUE,
      },
    },
  ];

  const FEATURE_VALUES = {
    [EXPERIMENT]: {
      prefs: {
        [PREFS[EXPERIMENT]]: {
          value: VALUES[EXPERIMENT],
          branch: USER,
        },
      },
    },
    [ROLLOUT]: {
      prefs: {
        [PREFS[ROLLOUT]]: {
          value: VALUES[ROLLOUT],
          branch: USER,
        },
      },
    },
  };

  for (const [i, { name, ...testCase }] of TEST_CASES.entries()) {
    info(`Running test case ${i}: ${name}`);

    const {
      setPrefsBefore,
      enrollmentOrder,
      expectedEnrollments,
      expectedUnenrollments,
      expectedPrefs,
    } = testCase;

    const { manager, cleanup } = await setupTest();

    info("Setting initial values of prefs...");
    setPrefs(setPrefsBefore);

    info("Enrolling...");
    for (const slug of enrollmentOrder) {
      await NimbusTestUtils.enrollWithFeatureConfig(
        {
          featureId: FEATURE_ID,
          value: FEATURE_VALUES[slug],
        },
        {
          manager,
          slug,
          isRollout: slug === ROLLOUT,
        }
      );
    }

    info("Checking expected enrollments...");
    for (const slug of expectedEnrollments) {
      const enrollment = manager.store.get(slug);
      Assert.ok(enrollment.active, `The enrollment for ${slug} is active`);
    }

    info("Checking expected unenrollments...");
    for (const slug of expectedUnenrollments) {
      const enrollment = manager.store.get(slug);
      Assert.ok(!enrollment.active, "The enrollment is no longer active.");
    }

    info("Checking expected prefs...");
    checkExpectedPrefs(expectedPrefs);

    info("Unenrolling...");
    if (expectedEnrollments.includes(ROLLOUT)) {
      await manager.unenroll(ROLLOUT);
    }
    if (expectedEnrollments.includes(EXPERIMENT)) {
      await manager.unenroll(EXPERIMENT);
    }

    info("Cleaning up...");
    Services.prefs.deleteBranch(PREFS[ROLLOUT]);
    Services.prefs.deleteBranch(PREFS[EXPERIMENT]);

    await cleanup();
  }
});

add_task(async function test_prefFlips_failed_experiment_and_rollout_2() {
  const ROLLOUT = "rollout";
  const EXPERIMENT = "experiment";

  const PREFS = {
    [ROLLOUT]: "test.nimbus.prefs.rollout-2",
    [EXPERIMENT]: "test.nimbus.prefs.experiment-2",
  };

  const VALUES = {
    [ROLLOUT]: "rollout-value",
    [EXPERIMENT]: "experiment-value",
  };

  const BOGUS_VALUE = 123;

  const TEST_CASES = [
    {
      name: "Enrolling in a rollout and then an experiment with errors",
      setPrefsBefore: {
        [PREFS[EXPERIMENT]]: { defaultBranchValue: BOGUS_VALUE },
      },
      enrollmentOrder: [ROLLOUT, EXPERIMENT],
      expectedEnrollments: [ROLLOUT],
      expectedUnenrollments: [EXPERIMENT],
      expectedPrefs: {
        [PREFS[ROLLOUT]]: VALUES[ROLLOUT],
        [PREFS[EXPERIMENT]]: BOGUS_VALUE,
      },
    },
  ];

  const FEATURE_VALUES = {
    [EXPERIMENT]: {
      prefs: {
        [PREFS[EXPERIMENT]]: {
          value: VALUES[EXPERIMENT],
          branch: USER,
        },
      },
    },
    [ROLLOUT]: {
      prefs: {
        [PREFS[ROLLOUT]]: {
          value: VALUES[ROLLOUT],
          branch: USER,
        },
      },
    },
  };

  for (const [i, { name, ...testCase }] of TEST_CASES.entries()) {
    info(`Running test case ${i}: ${name}`);

    const {
      setPrefsBefore,
      enrollmentOrder,
      expectedEnrollments,
      expectedUnenrollments,
      expectedPrefs,
    } = testCase;

    const { manager, cleanup } = await setupTest();

    info("Setting initial values of prefs...");
    setPrefs(setPrefsBefore);

    info("Enrolling...");
    for (const slug of enrollmentOrder) {
      await NimbusTestUtils.enrollWithFeatureConfig(
        {
          featureId: FEATURE_ID,
          value: FEATURE_VALUES[slug],
        },
        {
          manager,
          slug,
          isRollout: slug === ROLLOUT,
        }
      );
    }

    info("Checking expected enrollments...");
    for (const slug of expectedEnrollments) {
      const enrollment = manager.store.get(slug);
      Assert.ok(enrollment.active, `The enrollment for ${slug} is active`);
    }

    info("Checking expected unenrollments...");
    for (const slug of expectedUnenrollments) {
      const enrollment = manager.store.get(slug);
      Assert.ok(!enrollment.active, "The enrollment is no longer active.");
    }

    info("Checking expected prefs...");
    checkExpectedPrefs(expectedPrefs);

    info("Unenrolling...");
    if (expectedEnrollments.includes(ROLLOUT)) {
      await manager.unenroll(ROLLOUT);
    }
    if (expectedEnrollments.includes(EXPERIMENT)) {
      await manager.unenroll(EXPERIMENT);
    }

    info("Cleaning up...");
    Services.prefs.deleteBranch(PREFS[ROLLOUT]);
    Services.prefs.deleteBranch(PREFS[EXPERIMENT]);

    await cleanup();
  }
});

add_task(async function test_prefFlips_update_failure() {
  const { manager, cleanup } = await setupTest();

  PrefUtils.setPref("pref.one", "default-value", { branch: DEFAULT });
  PrefUtils.setPref("pref.two", "default-value", { branch: DEFAULT });

  const cleanupExperiment = await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: FEATURE_ID,
      value: {
        prefs: {
          "pref.one": { value: "one", branch: USER },
          "pref.two": { value: "two", branch: USER },
        },
      },
    },
    { manager, isRollout: true, slug: "rollout" }
  );

  Assert.equal(Services.prefs.getStringPref("pref.one"), "one");
  Assert.equal(Services.prefs.getStringPref("pref.two"), "two");

  await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: FEATURE_ID,
      value: {
        prefs: {
          "pref.one": { value: "experiment-value", branch: USER },
          "pref.two": { value: 2, branch: USER },
        },
      },
    },
    { manager, slug: "experiment" }
  );

  const rolloutEnrollment = manager.store.get("rollout");
  const experimentEnrollment = manager.store.get("experiment");

  Assert.ok(rolloutEnrollment.active, "Rollout is active");
  Assert.ok(!experimentEnrollment.active, "Experiment is inactive");
  Assert.equal(experimentEnrollment.unenrollReason, "prefFlips-failed");

  Assert.equal(Services.prefs.getStringPref("pref.one"), "one");
  Assert.equal(Services.prefs.getStringPref("pref.two"), "two");

  cleanupExperiment();

  Services.prefs.deleteBranch("pref.one");
  Services.prefs.deleteBranch("pref.two");

  await cleanup();
});

add_task(async function test_prefFlips_restore() {
  let storePath;

  const PREF_1 = "pref.one";
  const PREF_2 = "pref.two";
  const PREF_3 = "pref.three";
  const PREF_4 = "pref.FOUR";

  {
    const store = NimbusTestUtils.stubs.store();
    await store.init();

    store.addEnrollment(
      NimbusTestUtils.factories.rollout.withFeatureConfig(
        "rollout-1",
        {
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_1]: { branch: USER, value: PREF_1 },
            },
          },
        },
        {
          prefFlips: {
            originalValues: {
              [PREF_1]: null,
            },
          },
        }
      )
    );

    store.addEnrollment(
      NimbusTestUtils.factories.rollout.withFeatureConfig(
        "rollout-2",
        {
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_2]: { branch: USER, value: PREF_2 },
            },
          },
        },
        {
          prefFlips: {
            originalValues: {
              [PREF_2]: "original-pref-2-value",
            },
          },
        }
      )
    );

    store.addEnrollment(
      NimbusTestUtils.factories.rollout.withFeatureConfig(
        "rollout-3",
        {
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_3]: { branch: DEFAULT, value: PREF_3 },
            },
          },
        },
        {
          prefFlips: {
            originalValues: {
              [PREF_3]: null,
            },
          },
        }
      )
    );

    store.addEnrollment(
      NimbusTestUtils.factories.rollout.withFeatureConfig(
        "rollout-4",
        {
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF_4]: { branch: DEFAULT, value: PREF_4 },
            },
          },
        },
        {
          prefFlips: {
            originalValues: {
              [PREF_4]: "original-pref-4-value",
            },
          },
        }
      )
    );

    storePath = await NimbusTestUtils.saveStore(store);
  }

  const { manager, cleanup } = await setupTest({ storePath });

  Assert.ok(manager.store.get("rollout-1").active, "rollout-1 is active");
  Assert.ok(manager.store.get("rollout-2").active, "rollout-2 is active");
  Assert.ok(manager.store.get("rollout-3").active, "rollout-3 is active");
  Assert.ok(manager.store.get("rollout-4").active, "rollout-4 is active");

  Assert.equal(
    Services.prefs.getStringPref(PREF_1),
    PREF_1,
    `${PREF_1} has the correct value`
  );
  Assert.equal(
    Services.prefs.getStringPref(PREF_2),
    PREF_2,
    `${PREF_2} has the correct value`
  );
  Assert.equal(
    Services.prefs.getStringPref(PREF_3),
    PREF_3,
    `${PREF_3} has the correct value`
  );
  Assert.equal(
    Services.prefs.getStringPref(PREF_4),
    PREF_4,
    `${PREF_4} has the correct value`
  );

  await NimbusTestUtils.cleanupManager(
    ["rollout-1", "rollout-2", "rollout-3", "rollout-4"],
    { manager }
  );

  Assert.equal(
    PrefUtils.getPref(PREF_1),
    null,
    `${PREF_1} has the correct value after unenrollment`
  );
  Assert.equal(
    PrefUtils.getPref(PREF_2),
    "original-pref-2-value",
    `${PREF_2} has the correct value after unenrollment`
  );
  Assert.equal(
    PrefUtils.getPref(PREF_3),
    PREF_3,
    `${PREF_3} has the correct value after unenrollment (can't reset default branch)`
  );
  Assert.equal(
    PrefUtils.getPref(PREF_4),
    "original-pref-4-value",
    `${PREF_4} has the correct value`
  );

  await cleanup();
});

add_task(async function test_prefFlips_restore_failure_conflict() {
  let storePath;

  const PREF = "pref.foo.bar";
  {
    const store = NimbusTestUtils.stubs.store();
    await store.init();

    store.addEnrollment(
      NimbusTestUtils.factories.rollout.withFeatureConfig(
        "rollout-1",
        {
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF]: { branch: USER, value: "correct-value" },
            },
          },
        },
        {
          prefFlips: {
            originalValues: {
              [PREF]: null,
            },
          },
        }
      )
    );

    store.addEnrollment(
      NimbusTestUtils.factories.rollout.withFeatureConfig(
        "rollout-2",
        {
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF]: { branch: USER, value: "incorrect-value" },
            },
          },
        },
        {
          prefFlips: {
            originalValues: {
              [PREF]: null,
            },
          },
        }
      )
    );

    store.addEnrollment(
      NimbusTestUtils.factories.rollout.withFeatureConfig(
        "rollout-3",
        {
          featureId: FEATURE_ID,
          value: {
            prefs: {
              [PREF]: { branch: DEFAULT, value: "correct-value" },
            },
          },
        },
        {
          prefFlips: {
            originalValues: {
              [PREF]: null,
            },
          },
        }
      )
    );

    storePath = await NimbusTestUtils.saveStore(store);
  }

  const { manager, cleanup } = await setupTest({ storePath });

  Assert.ok(manager.store.get("rollout-1").active, "rollout-1 is active");
  Assert.ok(!manager.store.get("rollout-2").active, "rollout-2 is not active");
  Assert.equal(
    manager.store.get("rollout-2").unenrollReason,
    "prefFlips-failed"
  );
  Assert.ok(!manager.store.get("rollout-3").active, "rollout-3 is not active");
  Assert.equal(
    manager.store.get("rollout-3").unenrollReason,
    "prefFlips-failed"
  );

  Assert.equal(
    PrefUtils.getPref(PREF),
    "correct-value",
    `${PREF} has the correct value`
  );

  await NimbusTestUtils.cleanupManager(["rollout-1"], { manager });

  Assert.equal(
    PrefUtils.getPref(PREF),
    null,
    `${PREF} has the correct value after unenrollment`
  );

  await cleanup();
});

// Test the case where an experiment sets a default branch pref, but the user
// changed their user.js between restarts.
add_task(async function test_prefFlips_restore_failure_wrong_type() {
  const PREF_1 = "foo.bar.baz";
  const PREF_2 = "qux.quux.corge.grault";

  const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
    "prefFlips-test",
    {
      featureId: FEATURE_ID,
      value: {
        prefs: {
          [PREF_1]: {
            branch: DEFAULT,
            value: "recipe-value",
          },
          [PREF_2]: {
            branch: USER,
            value: "recipe-value",
          },
        },
      },
    }
  );

  let storePath;
  {
    const prevEnrollment = {
      slug: recipe.slug,
      branch: recipe.branches[0],
      active: true,
      experimentType: "nimbus",
      userFacingName: recipe.userFacingName,
      userFacingDescription: recipe.userFacingDescription,
      featureIds: recipe.featureIds,
      isRollout: recipe.isRollout,
      localizations: recipe.localizations,
      source: "rs-loader",
      prefFlips: {
        originalValues: {
          [PREF_1]: "original-value",
          [PREF_2]: "original-value",
        },
      },
      lastSeen: new Date().toJSON(),
    };

    const store = NimbusTestUtils.stubs.store();
    await store.init();
    store.set(prevEnrollment.slug, prevEnrollment);
    storePath = await NimbusTestUtils.saveStore(store);
  }

  Services.prefs.setIntPref(PREF_1, 123);

  const { manager, cleanup } = await setupTest({ storePath });

  const enrollment = manager.store.get(recipe.slug);

  Assert.ok(!enrollment.active, "Enrollment should be inactive");
  Assert.equal(enrollment.unenrollReason, "prefFlips-failed");

  Assert.ok(
    !Services.prefs.prefHasDefaultValue(PREF_1),
    `${PREF_1} has no default value`
  );
  Assert.ok(
    !Services.prefs.prefHasDefaultValue(PREF_2),
    `${PREF_2} has no default value`
  );
  Assert.equal(
    Services.prefs.getIntPref(PREF_1),
    123,
    `${PREF_1} value unchanged`
  );
  Assert.ok(!Services.prefs.prefHasUserValue(PREF_2), `${PREF_2} has no value`);

  Services.prefs.deleteBranch(PREF_1);
  Services.prefs.deleteBranch(PREF_2);
  await cleanup();
});

add_task(
  async function test_prefFlips_reenroll_set_default_branch_wrong_type() {
    const PREF = "test.pref.please.ignore";

    const recipe = NimbusTestUtils.factories.recipe.withFeatureConfig(
      "prefFlips-test",
      {
        featureId: FEATURE_ID,
        value: {
          prefs: {
            [PREF]: { value: 123, branch: DEFAULT },
          },
        },
      },
      { isRollout: true }
    );

    const { manager, cleanup } = await setupTest();

    PrefUtils.setPref(PREF, "default-value", { branch: DEFAULT });

    await manager.enroll(recipe, "rs-loader");

    let enrollment = manager.store.get(recipe.slug);

    Assert.ok(!enrollment.active, "enrollment should not be active");
    Assert.equal(enrollment.unenrollReason, "prefFlips-failed");

    await manager.enroll(recipe, "rs-loader", { reenroll: true });
    enrollment = manager.store.get(recipe.slug);

    Assert.ok(!enrollment.active, "enrollment should not be active");
    Assert.equal(enrollment.unenrollReason, "prefFlips-failed");

    Services.prefs.deleteBranch(PREF);

    await cleanup();
  }
);
