/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  NimbusTelemetry: "resource://nimbus/lib/Telemetry.sys.mjs",
  UnenrollmentCause: "resource://nimbus/lib/ExperimentManager.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.importESModule(
    "resource://messaging-system/lib/Logger.sys.mjs"
  );

  return new Logger("FirefoxLabs");
});

const IS_MAIN_PROCESS =
  Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT;

export class FirefoxLabs {
  #recipes;

  /**
   * Construct a new FirefoxLabs instance from the given set of recipes.
   *
   * @param {object[]} recipes The opt-in recipes to use.
   *
   * NB: You shiould use FirefoxLabs.create() directly instead of calling this constructor.
   */
  constructor(recipes) {
    this.#recipes = new Map(recipes.map(recipe => [recipe.slug, recipe]));
  }

  /**
   * Create a new FirefoxLabs instance with all available opt-in recipes that match targeting and
   * bucketing.
   */
  static async create() {
    if (!IS_MAIN_PROCESS) {
      throw new Error("FirefoxLabs can only be created in the main process");
    }

    const recipes = await lazy.ExperimentAPI.manager.getAllOptInRecipes();
    return new FirefoxLabs(recipes);
  }

  /**
   * Enroll in an opt-in.
   *
   * @param {string} slug The slug of the opt-in to enroll.
   * @param {string} branchSlug The slug of the branch to enroll in.
   */
  async enroll(slug, branchSlug) {
    if (!slug || !branchSlug) {
      throw new TypeError("enroll: slug and branchSlug are required");
    }

    const recipe = this.#recipes.get(slug);
    if (!recipe) {
      lazy.log.error(`No recipe found with slug ${slug}`);
      return;
    }

    if (!recipe.branches.find(branch => branch.slug === branchSlug)) {
      lazy.log.error(
        `Failed to enroll in ${slug} ${branchSlug}: branch does not exist`
      );
      return;
    }

    try {
      await lazy.ExperimentAPI.manager.enroll(recipe, "rs-loader", {
        branchSlug,
      });
    } catch (e) {
      lazy.log.error(`Failed to enroll in ${slug} (branch ${branchSlug})`, e);
    }
  }

  /**
   * Unenroll from a opt-in.
   *
   * @param {string} slug The slug of the opt-in to unenroll.
   */
  unenroll(slug) {
    if (!slug) {
      throw new TypeError("slug is required");
    }

    if (!this.#recipes.has(slug)) {
      lazy.log.error(`Unknown opt-in ${slug}`);
      return;
    }

    try {
      lazy.ExperimentAPI.manager.unenroll(
        slug,
        lazy.UnenrollmentCause.fromReason(
          lazy.NimbusTelemetry.UnenrollReason.LABS_OPT_OUT
        )
      );
    } catch (e) {
      lazy.log.error(`unenroll: failed to unenroll from ${slug}`, e);
    }
  }

  /**
   * Return the number of eligible opt-ins.
   *
   * @return {number} The number of eligible opt-ins.
   */
  get count() {
    return this.#recipes.size;
  }

  /**
   * Yield all available opt-ins.
   *
   * @yields {object} The opt-ins.
   */
  *all() {
    for (const recipe of this.#recipes.values()) {
      yield recipe;
    }
  }

  /**
   * Return an opt-in by its slug
   *
   * @param {string} slug The slug of the opt-in to return.
   *
   * @returns {object} The requested opt-in, if it exists.
   */
  get(slug) {
    return this.#recipes.get(slug);
  }
}
