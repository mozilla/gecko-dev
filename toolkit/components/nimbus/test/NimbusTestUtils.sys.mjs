/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** @import { NimbusEnrollments } from "../lib/Enrollments.sys.mjs" */

import {
  ExperimentAPI,
  NimbusFeatures,
} from "resource://nimbus/ExperimentAPI.sys.mjs";
import { ExperimentStore } from "resource://nimbus/lib/ExperimentStore.sys.mjs";
import { FileTestUtils } from "resource://testing-common/FileTestUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FeatureManifest: "resource://nimbus/FeatureManifest.sys.mjs",
  JsonSchema: "resource://gre/modules/JsonSchema.sys.mjs",
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
  NimbusEnrollments: "resource://nimbus/lib/Enrollments.sys.mjs",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.sys.mjs",
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  ProfilesDatastoreService:
    "moz-src:///toolkit/profile/ProfilesDatastoreService.sys.mjs",
  RemoteSettingsExperimentLoader:
    "resource://nimbus/lib/RemoteSettingsExperimentLoader.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

function fetchSchemaSync(uri) {
  // Yes, this is doing a sync load, but this is only done *once* and we cache
  // the result after *and* it is test-only.
  const channel = lazy.NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  });
  const stream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
    Ci.nsIScriptableInputStream
  );

  stream.init(channel.open());

  const available = stream.available();
  const json = stream.read(available);
  stream.close();

  return JSON.parse(json);
}

ChromeUtils.defineLazyGetter(lazy, "enrollmentSchema", () => {
  return fetchSchemaSync(
    "resource://testing-common/nimbus/schemas/NimbusEnrollment.schema.json"
  );
});

ChromeUtils.defineLazyGetter(lazy, "featureSchema", () => {
  return fetchSchemaSync(
    "resource://testing-common/nimbus/schemas/ExperimentFeature.schema.json"
  );
});

const { SYNC_DATA_PREF_BRANCH, SYNC_DEFAULTS_PREF_BRANCH } = ExperimentStore;

async function fetchSchema(url) {
  const response = await fetch(url);
  const schema = await response.json();
  if (!schema) {
    throw new Error(`Failed to load ${url}`);
  }
  return schema;
}

function validateSchema(schemaOrValidator, value, errorMsg) {
  const validator =
    schemaOrValidator instanceof lazy.JsonSchema.Validator
      ? schemaOrValidator
      : new lazy.JsonSchema.Validator(schemaOrValidator);

  const result = validator.validate(value, { shortCircuit: false });
  if (result.errors.length) {
    throw new Error(
      `${errorMsg}: ${JSON.stringify(result.errors, undefined, 2)}`
    );
  }
  return value;
}

function validateFeatureValueEnum({ branch }) {
  let { features } = branch;
  for (let feature of features) {
    // If we're not using a real feature skip this check
    if (!lazy.FeatureManifest[feature.featureId]) {
      return;
    }
    let { variables } = lazy.FeatureManifest[feature.featureId];
    for (let varName of Object.keys(variables)) {
      let varValue = feature.value[varName];
      if (
        varValue &&
        variables[varName].enum &&
        !variables[varName].enum.includes(varValue)
      ) {
        throw new Error(
          `${varName} should have one of the following values: ${JSON.stringify(
            variables[varName].enum
          )} but has value '${varValue}'`
        );
      }
    }
  }
}

let _testSuite = null;

export const NimbusTestUtils = {
  init(testCase) {
    _testSuite = testCase;

    Object.defineProperty(NimbusTestUtils, "Assert", {
      configurable: true,
      get: () => _testSuite.Assert,
    });
  },

  get Assert() {
    // This gets replaced in NimbusTestUtils.init().
    throw new Error("You must call NimbusTestUtils.init(this)");
  },

  assert: {
    /**
     * Assert that the store has no active enrollments and then clean up the
     * store.
     *
     * This function will also clean up the isEarlyStartup cache.
     *
     * @param {object} store
     *        The `ExperimentStore`.
     */
    async storeIsEmpty(store) {
      NimbusTestUtils.Assert.deepEqual(
        store
          .getAll()
          .filter(e => e.active)
          .map(e => e.slug),
        [],
        "Store should have no active enrollments"
      );

      // Do *not* queue a removal from the store yet -- we'll handle that in
      // cleanupEnrollmentDatabase.
      store
        .getAll()
        .filter(e => !e.active)
        .forEach(e =>
          store._deleteForTests(e.slug, { removeFromNimbusEnrollments: false })
        );

      NimbusTestUtils.Assert.deepEqual(
        store
          .getAll()
          .filter(e => !e.active)
          .map(e => e.slug),
        [],
        "Store should have no inactive enrollments"
      );

      NimbusTestUtils.cleanupStorePrefCache();

      await NimbusTestUtils.cleanupEnrollmentDatabase(store?._db);
    },

    /**
     * Assert that an enrollment exists in the NimbusEnrollments table.
     *
     * @param {string} slug The slug to check for.
     *
     * @param {object} options
     *
     * @param {boolean | undefined} options.active If provided, this function
     * will assert that the enrollment is active (if true) or inactive (if
     * false).
     *
     * @param {string} options.profileId The profile ID to query with. Defaults
     * to the current profile ID.
     */
    async enrollmentExists(
      slug,
      { active: expectedActive, profileId = ExperimentAPI.profileId } = {}
    ) {
      const conn = await lazy.ProfilesDatastoreService.getConnection();

      const result = await conn.execute(
        `
        SELECT
          active,
          unenrollReason
          FROM NimbusEnrollments
        WHERE
          slug = :slug AND
          profileId = :profileId;
        `,
        { slug, profileId }
      );

      NimbusTestUtils.Assert.ok(
        result.length === 1,
        `Enrollment for ${slug} in profile ${profileId} exists`
      );

      if (typeof expectedActive === "boolean") {
        const active = result[0].getResultByName("active");
        const unenrollReason = result[0].getResultByName("unenrollReason");

        NimbusTestUtils.Assert.equal(
          expectedActive,
          active,
          `Enrollment for ${slug} is ${expectedActive} -- unenrollReason = ${unenrollReason}`
        );
      }
    },

    /**
     * Assert that an enrollment does not exist in the NimbusEnrollments table.
     *
     * @param {string} slug The slug to check for.
     * @param {object} options
     * @param {string} options.profileId The profielID to query with. Defaults
     * to the current profile ID.
     */
    async enrollmentDoesNotExist(
      slug,
      { profileId = ExperimentAPI.profileId } = {}
    ) {
      const conn = await lazy.ProfilesDatastoreService.getConnection();

      const result = await conn.execute(
        `
          SELECT 1
          FROM NimbusEnrollments
          WHERE
            slug = :slug AND
            profileId = :profileId;
        `,
        { slug, profileId }
      );

      NimbusTestUtils.Assert.ok(
        result.length === 0,
        `Enrollment for ${slug} in profile ${profileId} does not exist`
      );
    },
  },

  factories: {
    /**
     * Create a experiment enrollment for an `ExperimentStore`.
     *
     * @param {string} slug
     *        The slug for the created enrollment.
     *
     * @param {object?} props
     *        Additional properties to splat into the created enrollment.
     */
    experiment(slug, props = {}) {
      const { isRollout = false } = props;

      const experimentType = isRollout ? "rollout" : "experiment";
      const userFacingName = `NimbusTestUtils ${experimentType}`;
      const userFacingDescription = `NimbusTestUtils ${experimentType}`;

      return {
        slug,
        active: true,
        branch: {
          slug: "treatment",
          ratio: 1,
          features: [
            {
              featureId: "testFeature",
              value: { testInt: 123, enabled: true },
            },
          ],
          firefoxLabsTitle: null,
        },
        source: "NimbusTestUtils",
        isEnrollmentPaused: true,
        experimentType,
        userFacingName,
        userFacingDescription,
        lastSeen: new Date().toJSON(),
        featureIds: props?.branch?.features?.map(f => f.featureId) ?? [
          "testFeature",
        ],
        isRollout: false,
        isFirefoxLabsOptIn: false,
        firefoxLabsTitle: null,
        firefoxLabsDescription: null,
        firefoxLabsDescriptionLinks: null,
        firefoxLabsGroup: null,
        requiresRestart: false,
        localizations: null,
        ...props,
      };
    },

    /**
     * Create a rollout enrollment for an `ExperimentStore`.
     *
     * @param {string} slug
     *        The slug for the created enrollment.
     *
     * @param {object?} props
     *        Additional properties to splat into the created enrollment.
     */
    rollout(slug, props = {}) {
      return NimbusTestUtils.factories.experiment(slug, {
        ...props,
        isRollout: true,
      });
    },

    /**
     * Create a recipe.
     *
     * @param {string} slug
     *        The slug for the created recipe.
     *
     * @param {object?} props
     *        Additional properties to splat into to the
     */
    recipe(slug, props = {}) {
      return {
        id: slug,
        schemaVersion: "1.7.0",
        appName: "firefox_desktop",
        appId: "firefox-desktop",
        channel: "nightly",
        slug,
        isEnrollmentPaused: false,
        probeSets: [],
        startDate: null,
        endDate: null,
        proposedEnrollment: 7,
        referenceBranch: "control",
        application: "firefox-desktop",
        branches: props?.isRollout
          ? [NimbusTestUtils.factories.recipe.branches[0]]
          : NimbusTestUtils.factories.recipe.branches,
        bucketConfig: NimbusTestUtils.factories.recipe.bucketConfig,
        userFacingName: "NimbusTestUtils recipe",
        userFacingDescription: "NimbusTestUtils recipe",
        featureIds: props?.branches?.[0].features?.map(f => f.featureId) || [
          "testFeature",
        ],
        targeting: "true",
        isRollout: false,
        isFirefoxLabsOptIn: false,
        firefoxLabsTitle: null,
        firefoxLabsDescription: null,
        firefoxLabsDescriptionLinks: null,
        firefoxLabsGroup: null,
        requiresRestart: false,
        localizations: null,
        ...props,
      };
    },
  },

  stubs: {
    store(path) {
      return new ExperimentStore("ExperimentStoreData", {
        path: path ?? FileTestUtils.getTempFile("test-experiment-store").path,
      });
    },

    manager(store) {
      const manager = new lazy.ExperimentManager({
        store: store ?? NimbusTestUtils.stubs.store(),
      });
      const addEnrollment = manager.store.addEnrollment.bind(manager.store);

      // We want calls to `store.addEnrollment` to implicitly validate the
      // enrollment before saving to store
      lazy.sinon
        .stub(manager.store, "addEnrollment")
        .callsFake((enrollment, recipe) => {
          NimbusTestUtils.validateEnrollment(enrollment);
          return addEnrollment(enrollment, recipe);
        });

      return manager;
    },

    rsLoader(manager) {
      const loader = new lazy.RemoteSettingsExperimentLoader(
        manager ?? NimbusTestUtils.stubs.manager()
      );

      Object.defineProperties(loader.remoteSettingsClients, {
        experiments: {
          value: {
            collectionName: "nimbus-desktop-experiments (stubbed)",
            get: () => Promise.resolve([]),
            db: { getLastModified: () => Promise.resolve(0) },
          },
        },

        secureExperiments: {
          value: {
            collectionName: "nimbus-secure-experiments (stubbed)",
            get: () => Promise.resolve([]),
            db: { getLastModified: () => Promise.resolve(0) },
          },
        },
      });

      return loader;
    },
  },

  /**
   * Add an enrollment to the store without going through the entire enroll
   * flow.
   *
   * Using `ExperimentAPI.manager.enroll()` or {@link NimbusTestUtils.enroll}
   * (or similar hlpers) should be preferred in most cases.
   *
   * N.B.: The JSON store will not be immediately saved to disk, nor will the
   * NimbusEnrollments table. You must call {@link NimbusTestUtils.saveStore} or
   * wait for it to save on its own.
   *
   * @param {object} recipe The recipe to add an enrollment for.
   * @param {object} options
   * @param {ExperimentStore} options.store The store to add the enrollment to.
   * Defaults to the global ExperimentStore (`ExperimentAPI.manager.store`).
   * @param {string} options.branchSlug The slug of the branch to enroll in.
   * Must be provided if there is more than once branch.
   * @param {object} options.extra Extra properties to override on the
   * enrollment object.
   *
   * @returns {object} The enrollment.
   */
  addEnrollmentForRecipe(recipe, { store, branchSlug, extra = {} } = {}) {
    let branch;
    if (branchSlug) {
      branch = recipe.branches.find(b => b.slug === branchSlug);
    } else if (recipe.branches.length === 1) {
      branch = recipe.branches[0];
    } else {
      throw new Error("branchSlug required for recipes with > 1 branch");
    }

    if (!branch) {
      throw new Error("No branch");
    }

    const enrollment = {
      slug: recipe.slug,
      branch,
      active: true,
      source: "NimbusTestUtils",
      userFacingName: recipe.userFacingName,
      userFacingDescription: recipe.userFacingDescription,
      lastSeen: new Date().toJSON(),
      featureIds: recipe.featureIds,
      isRollout: recipe.isRollout,
      isFirefoxLabsOptIn: recipe.isFirefoxLabsOptIn,
      firefoxLabsTitle: recipe.firefoxLabsTitle,
      firefoxLabsDescription: recipe.firefoxLabsDescription,
      firefoxLabsDescriptionLinks: recipe.firefoxLabsDescriptionLinks,
      firefoxLabsGroup: recipe.firefoxLabsGroup,
      requiresRestart: recipe.requiresRestart,
      localizations: recipe.localizations ?? null,
      ...extra,
    };

    (store ?? ExperimentAPI.manager.store).addEnrollment(enrollment, recipe);

    return enrollment;
  },

  /**
   * Add features for tests.
   *
   * NB: These features will only be visible to the JS Nimbus client. The native
   * Nimbus client will have no access.
   *
   * @params {...object} features
   *         A list of `_NimbusFeature`s.
   *
   * @returns {function(): void}
   *          A cleanup function to remove the features once the test has completed.
   */
  addTestFeatures(...features) {
    const validator = new lazy.JsonSchema.Validator(lazy.featureSchema);

    for (const feature of features) {
      if (Object.hasOwn(NimbusFeatures, feature.featureId)) {
        throw new Error(
          `Cannot add feature ${feature.featureId} -- a feature with this ID already exists!`
        );
      }

      // Stub out metadata-only properties.
      feature.manifest.owner ??= "owner@example.com";
      feature.manifest.description ??= `${feature.featureId} description`;
      feature.manifest.hasExposure ??= false;
      feature.manifest.exposureDescription ??= `${feature.featureId} exposure description`;

      feature.manifest.variables ??= {};
      for (const [name, variable] of Object.entries(
        feature.manifest?.variables
      )) {
        variable.description ??= `${name} variable`;
      }

      validateSchema(
        validator,
        feature.manifest,
        `Could not validate feature ${feature.featureId}`
      );
    }

    for (const feature of features) {
      NimbusFeatures[feature.featureId] = feature;
    }

    return () => {
      for (const { featureId } of features) {
        delete NimbusFeatures[featureId];
      }
    };
  },

  /**
   * Unenroll from all the given slugs and assert that the store is now empty.
   *
   * @params {string[]} slugs
   *         The slugs to unenroll from.
   *
   * @params {object?} options
   *
   * @params {object?} options.manager
   *         The ExperimentManager to clean up. Defaults to the global
   *         ExperimentManager.
   *
   * @returns {Promise<void>}
   *          A promise that resolves when all experiments have been unenrolled
   *          and the store is empty.
   */
  async cleanupManager(slugs, { manager } = {}) {
    const experimentManager = manager ?? ExperimentAPI.manager;

    for (const slug of slugs) {
      experimentManager.unenroll(slug);
    }

    await NimbusTestUtils.assert.storeIsEmpty(experimentManager.store);
  },

  /**
   * Clean up the enrollments database, removing all enrollments for the current
   * profile ID.
   *
   * The database flushing task will be finalized, preventing further writes.
   *
   * @param {NimbusEnrollments} db The NimbusEnrollments object.
   */
  async cleanupEnrollmentDatabase(db) {
    if (!lazy.NimbusEnrollments.databaseEnabled) {
      // We are in an xpcshell test that has not initialized the
      // ProfilesDatastoreService.
      //
      // TODO(bug 1967779): require the ProfilesDatastoreService to be initialized
      // and remove this check.
      return;
    }

    // Wait for all pending writes to complete and clean up shutdown blocker
    // state.
    await db.finalize();

    const profileId = ExperimentAPI.profileId;

    const conn = await lazy.ProfilesDatastoreService.getConnection();

    const activeSlugs = await conn
      .execute(
        `
        SELECT
          slug
        FROM NimbusEnrollments
        WHERE
          profileId = :profileId AND
          active = true;
      `,
        { profileId }
      )
      .then(rows => rows.map(row => row.getResultByName("slug")));

    NimbusTestUtils.Assert.deepEqual(
      activeSlugs,
      [],
      `No active slugs in NimbusEnrollments for ${profileId}`
    );

    await conn.execute(
      `
        DELETE FROM NimbusEnrollments
        WHERE
          profileId = :profileId AND
          active = false;
      `,
      { profileId }
    );
  },

  /**
   * Cleanup any isEarlyStartup features cached in prefs.
   */
  cleanupStorePrefCache() {
    // These may throw if nothing is cached, but it is harmless.

    try {
      Services.prefs.deleteBranch(SYNC_DATA_PREF_BRANCH);
    } catch (e) {}
    try {
      Services.prefs.deleteBranch(SYNC_DEFAULTS_PREF_BRANCH);
    } catch (e) {}
  },

  /**
   * Enroll in the given recipe.
   *
   * @param {object} recipe
   *        The recipe to enroll in.
   *
   * @param {object?} options
   *
   * @param {object?} options.manager
   *        The ExperimentManager to use for enrollment. If not provided, the
   *        global ExperimentManager will be used.
   *
   * @param {string?} options.source
   *        The source to attribute to the enrollment.
   *
   * @returns {Promise<function(): Promise<void>>}
   *          A cleanup function that will unenroll from the enrolled recipe and
   *          remove it from the store.
   *
   * @throws {Error} If the recipe references a feature that does not exist or
   *                 if the recipe fails to enroll.
   */
  async enroll(recipe, { manager, source = "nimbus-test-utils" } = {}) {
    const experimentManager = manager ?? ExperimentAPI.manager;

    if (!recipe?.slug) {
      throw new Error("Experiment with slug is required");
    }

    for (const featureId of recipe.featureIds) {
      if (!Object.hasOwn(NimbusFeatures, featureId)) {
        throw new Error(
          `Refusing to enroll in ${recipe.slug}: feature ${featureId} does not exist`
        );
      }
    }

    await experimentManager.store.ready();

    const enrollment = await experimentManager.enroll(recipe, source);

    if (!enrollment) {
      throw new Error(`Failed to enroll in ${recipe}`);
    }

    experimentManager.store._syncToChildren({ flush: true });

    return async function doEnrollmentCleanup() {
      experimentManager.unenroll(enrollment.slug);
      experimentManager.store._deleteForTests(enrollment.slug);

      await NimbusTestUtils.flushStore(experimentManager.store);
    };
  },

  /**
   * Enroll in an automatically-generated recipe with the given feature
   * configuration.
   *
   * @param {object} featureConfig
   *
   * @param {string} featureConfig.featureId
   *                 The name of the feature.
   *
   * @param {object} featureConfig.value
   *                 The feature value.
   *
   * @param {object?} options
   *
   * @param {object?} options.manager
   *        The ExperimentManager to use for enrollment. If not provided, the
   *        global ExperimentManager will be used.
   *
   * @param {string?} options.source
   *        The source to attribute to the enrollment.
   *
   * @param {branchSlug?} options.slug
   *        The slug to use for the recipe. If not provided one will be
   *        generated based on `featureId`.
   *
   * @param {string?} options.branchSlug
   *        The slug to use for the enrolled branch. Defaults to "control".
   *
   * @param {boolean?} options.isRollout
   *        If true, the enrolled recipe will be a rollout.
   *
   * @returns {Promise<function(): Promise<void>>}
   *          A cleanup function that will unenroll from the enrolled recipe and
   *          remove it from the store.
   *
   * @throws {Error} If the feature does not exist.
   */
  async enrollWithFeatureConfig(
    { featureId, value = {} },
    { manager, source, slug, branchSlug = "control", isRollout = false } = {}
  ) {
    const experimentManager = manager ?? ExperimentAPI.manager;
    await experimentManager.store.ready();

    const experimentType = isRollout ? "rollout" : "experiment";
    const experimentId =
      slug ?? `${featureId}-${experimentType}-${Math.random()}`;

    const recipe = NimbusTestUtils.factories.recipe(experimentId, {
      bucketConfig: {
        ...NimbusTestUtils.factories.recipe.bucketConfig,
        count: 1000,
      },
      branches: [
        {
          slug: branchSlug,
          ratio: 1,
          features: [{ featureId, value }],
        },
      ],
      isRollout,
    });

    return NimbusTestUtils.enroll(recipe, {
      manager: experimentManager,
      source,
    });
  },

  /**
   *
   * @param {string} slug
   * @param {object} options
   * @param {string} options.profileId
   */
  async queryEnrollment(slug, { profileId } = {}) {
    const conn = await lazy.ProfilesDatastoreService.getConnection();
    const result = await conn.execute(
      `
      SELECT
        profileId,
        slug,
        branchSlug,
        json(recipe) AS recipe,
        active,
        unenrollReason,
        lastSeen,
        json(setPrefs) AS setPrefs,
        json(prefFlips) AS prefFlips,
        source
      FROM NimbusEnrollments
      WHERE
        slug = :slug AND
        profileId = :profileId;
      `,
      {
        slug,
        profileId: profileId ?? ExperimentAPI.profileId,
      }
    );

    if (!result.length) {
      return null;
    }

    const row = result[0];

    const fields = [
      "profileId",
      "slug",
      "branchSlug",
      "recipe",
      "active",
      "unenrollReason",
      "lastSeen",
      "setPrefs",
      "prefFlips",
      "source",
    ];

    const enrollment = {};

    for (const field of fields) {
      enrollment[field] = row.getResultByName(field);
    }

    enrollment.recipe = JSON.parse(enrollment.recipe);
    enrollment.setPrefs = JSON.parse(enrollment.setPrefs);
    enrollment.prefFlips = JSON.parse(enrollment.prefFlips);

    return enrollment;
  },

  /**
   * Remove the ExperimentStore file.
   *
   * If the store contains active enrollments this function will cause the test
   * to fail.
   *
   * @params {ExperimentStore} store
   *         The store to delete.
   */
  async removeStore(store) {
    await NimbusTestUtils.assert.storeIsEmpty(store);

    // Prevent the next save from happening.
    store._store._saver.disarm();

    // If we're too late to stop the save from happening then we need to wait
    // for it to finish. Otherwise the saver might recreate the file on disk
    // after we delete it.
    if (store._store._saver.isRunning) {
      await store._store._saver._runningPromise;
    }

    await IOUtils.remove(store._store.path);
  },

  /**
   * Save the store to disk.
   *
   * This will also flush the NimbusEnrollments table.
   *
   * @param {ExperimentStore} store
   *        The store to save.
   *
   * @returns {string} The path to the file on disk.
   */
  async saveStore(store) {
    const jsonFile = store._store;

    if (jsonFile._saver.isRunning) {
      // It is possible that the store has been updated since we started writing
      // to disk. If we've already started writing, wait for that to finish.
      await jsonFile._saver._runningPromise;
    } else if (jsonFile._saver.isArmed) {
      // Otherwise, if we have a pending write we cancel it.
      jsonFile._saver.disarm();
    }

    await jsonFile._save();
    await store._db?._flushNow();

    return store._store.path;
  },

  /**
   * @typedef {object} TestContext
   *
   * @property {object} sandbox
   *           A sinon sandbox.
   *
   * @property {RemoteSettingsExperimentLoader} loader
   *           A RemoteSettingsExperimentLoader instance that has stubbed
   *           RemoteSettings clients.
   *
   * @property {ExperimentManager} manager
   *           An ExperimentManager instance that will validate all enrollments
   *           added to its store.
   *
   * @property {(function(): void)?} initExperimentAPI
   *           A function that will complete ExperimentAPI initialization.
   *
   * @property {function(): Promise<void>} cleanup
   *           A cleanup function that should be called at the end of the test.
   */

  /**
   * @param {object?} options
   * @param {boolean?} options.init
   *        Initialize the Experiment API.
   *
   *        If false, the returned context will return an `initExperimentAPI` member that
   *        will complete the initialization.
   *
   * @param {string?} options.storePath
   *        An optional path to an existing ExperimentStore to use for the
   *        ExperimentManager.
   *
   * @param {object[]?} options.experiments
   *        If provided, these recipes will be returned by the RemoteSettings
   *        experiments client.
   *
   * @param {object[]?} options.secureExperiments
   *        If provided, these recipes will be returned by the RemoteSetings
   *        secureExperiments client.
   *
   * @param {boolean?} clearTelemetry
   *        If true, telemetry will be reset in the cleanup function.
   *
   * @returns {TestContext}
   *          Everything you need to write a test using Nimbus.
   */
  async setupTest({
    init = true,
    storePath,
    experiments,
    secureExperiments,
    clearTelemetry = false,
    features,
  } = {}) {
    const sandbox = lazy.sinon.createSandbox();

    let cleanupFeatures = null;
    if (Array.isArray(features)) {
      cleanupFeatures = NimbusTestUtils.addTestFeatures(...features);
    }

    const store = NimbusTestUtils.stubs.store(storePath);
    const manager = NimbusTestUtils.stubs.manager(store);
    const loader = NimbusTestUtils.stubs.rsLoader(manager);

    sandbox.stub(ExperimentAPI, "_rsLoader").get(() => loader);
    sandbox.stub(ExperimentAPI, "manager").get(() => manager);
    sandbox
      .stub(loader.remoteSettingsClients.experiments, "get")
      .resolves(Array.isArray(experiments) ? experiments : []);
    sandbox
      .stub(loader.remoteSettingsClients.secureExperiments, "get")
      .resolves(Array.isArray(secureExperiments) ? secureExperiments : []);

    const ctx = {
      sandbox,
      loader,
      manager,
      store,
      async cleanup() {
        await NimbusTestUtils.assert.storeIsEmpty(manager.store);

        ExperimentAPI._resetForTests();
        sandbox.restore();

        if (cleanupFeatures) {
          cleanupFeatures();
        }

        if (clearTelemetry) {
          Services.fog.testResetFOG();
          Services.telemetry.clearEvents();
        }

        // Remove all migration state.
        Services.prefs.deleteBranch("nimbus.migrations.");
      },
    };

    const initExperimentAPI = () => ExperimentAPI.init();

    if (init) {
      await initExperimentAPI();
    } else {
      ctx.initExperimentAPI = initExperimentAPI;
    }

    return ctx;
  },

  /**
   * Validate an enrollment matches the Nimbus enrollment schema.
   *
   * @params {object} enrollment
   *         The enrollment to validate.
   *
   * @throws If the enrollment does not validate or its feature configurations
   *         contain invalid enum variants.
   */
  validateEnrollment(enrollment) {
    // We still have single feature experiment recipes for backwards
    // compatibility testing but we don't do schema validation
    if (!enrollment.branch.features && enrollment.branch.feature) {
      return;
    }

    validateFeatureValueEnum(enrollment);
    validateSchema(
      lazy.enrollmentSchema,
      enrollment,
      `Enrollment ${enrollment.slug} is not valid`
    );
  },

  /**
   * Validate the experiment matches the Nimbus experiment schema.
   *
   * @param {object} experiment
   *        The experiment to validate.
   *
   * @throws If the experiment does not validate or it includes unknown feature
   *         IDs.
   */
  async validateExperiment(experiment) {
    const schema = await fetchSchema(
      "resource://nimbus/schemas/NimbusExperiment.schema.json"
    );

    // Ensure that the `featureIds` field is properly set
    const { branches } = experiment;
    branches.forEach(branch => {
      branch.features.map(({ featureId }) => {
        if (!experiment.featureIds.includes(featureId)) {
          throw new Error(
            `Branch(${branch.slug}) contains feature(${featureId}) but that's not declared in recipe(${experiment.slug}).featureIds`
          );
        }
      });
    });

    validateSchema(
      schema,
      experiment,
      `Experiment ${experiment.slug} not valid`
    );
  },

  /**
   * Wait for the given slugs to be the only active enrollments in the
   * NimbusEnrollments table.
   *
   * @param {string[]} expectedSlugs The slugs of the only active enrollments we
   * expect.
   */
  async waitForActiveEnrollments(expectedSlugs) {
    const profileId = ExperimentAPI.profileId;

    await lazy.TestUtils.waitForCondition(async () => {
      const conn = await lazy.ProfilesDatastoreService.getConnection();
      const slugs = await conn
        .execute(
          `
            SELECT
              slug
            FROM NimbusEnrollments
            WHERE
              active = true AND
              profileId = :profileId;
          `,
          { profileId }
        )
        .then(rows => rows.map(row => row.getResultByName("slug")));

      return lazy.ObjectUtils.deepEqual(slugs.sort(), expectedSlugs.sort());
    }, `Waiting for enrollments of ${expectedSlugs} to sync to database`);
  },

  async waitForInactiveEnrollment(slug) {
    const profileId = ExperimentAPI.profileId;

    await lazy.TestUtils.waitForCondition(async () => {
      const conn = await lazy.ProfilesDatastoreService.getConnection();
      const result = await conn.execute(
        `
            SELECT
              active
            FROM NimbusEnrollments
            WHERE
              slug = :slug AND
              profileId = :profileId;
          `,
        { profileId, slug }
      );

      return result.length === 1 && !result[0].getResultByName("active");
    }, `Waiting for ${slug} enrollment to exist and be inactive`);
  },

  async waitForAllUnenrollments() {
    const profileId = ExperimentAPI.profileId;

    await lazy.TestUtils.waitForCondition(async () => {
      const conn = await lazy.ProfilesDatastoreService.getConnection();
      const slugs = await conn
        .execute(
          `
            SELECT
              slug
            FROM NimbusEnrollments
            WHERE
              active = true AND
              profileId = :profileId;
          `,
          { profileId }
        )
        .then(rows => rows.map(row => row.getResultByName("slug")));

      return slugs.length === 0;
    }, "Waiting for unenrollments to sync to database");
  },

  async flushStore(store = null) {
    const db = (store ?? ExperimentAPI.manager.store)._db;

    await db?._flushNow();
  },
};

Object.defineProperties(NimbusTestUtils.factories.experiment, {
  withFeatureConfig: {
    value: function NimbusTestUtils_factories_experiment_withFeatureConfig(
      slug,
      { branchSlug = "control", featureId, value = {} } = {},
      props = {}
    ) {
      return NimbusTestUtils.factories.experiment(slug, {
        branch: {
          slug: branchSlug,
          ratio: 1,
          features: [
            {
              featureId,
              value,
            },
          ],
        },
        ...props,
      });
    },
  },
});

Object.defineProperties(NimbusTestUtils.factories.rollout, {
  withFeatureConfig: {
    value: function NimbusTestUtils_factories_rollout_withFeatureConfig(
      slug,
      { branchSlug = "control", featureId, value = {} } = {},
      props = {}
    ) {
      return NimbusTestUtils.factories.rollout(slug, {
        branch: {
          slug: branchSlug,
          ratio: 1,
          features: [
            {
              featureId,
              value,
            },
          ],
        },
        ...props,
      });
    },
  },
});

Object.defineProperties(NimbusTestUtils.factories.recipe, {
  bucketConfig: {
    /**
     * A helper for generating valid bucketing configurations.
     *
     * This bucketing configuration will always result in enrollment.
     */
    get() {
      return {
        namespace: "nimbus-test-utils",
        randomizationUnit: "normandy_id",
        start: 0,
        count: 1000,
        total: 1000,
      };
    },
  },

  /**
   * A helper for generating experiment branches.
   */
  branches: {
    get() {
      return [
        {
          slug: "control",
          ratio: 1,
          features: [
            {
              featureId: "testFeature",
              value: { testInt: 123, enabled: true },
            },
          ],
          firefoxLabsTitle: null,
        },
        {
          slug: "treatment",
          ratio: 1,
          features: [
            {
              featureId: "testFeature",
              value: { testInt: 123, enabled: true },
            },
          ],
          firefoxLabsTitle: null,
        },
      ];
    },
  },

  /**
   * A helper for generating a recipe that has a single branch with the given
   * feature config.
   */
  withFeatureConfig: {
    value: function NimbusTestUtils_factories_recipe_withFeatureConfig(
      slug,
      { branchSlug = "control", featureId, value = {} } = {},
      props = {}
    ) {
      return NimbusTestUtils.factories.recipe(slug, {
        branches: [
          {
            slug: branchSlug,
            ratio: 1,
            features: [
              {
                featureId,
                value,
              },
            ],
            firefoxLabsTitle: null,
          },
        ],
        ...props,
      });
    },
  },
});
