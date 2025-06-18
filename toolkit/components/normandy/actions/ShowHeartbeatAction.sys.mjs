/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseAction } from "resource://normandy/actions/BaseAction.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ActionSchemas: "resource://normandy/actions/schemas/index.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  ClientEnvironment: "resource://normandy/lib/ClientEnvironment.sys.mjs",
  Heartbeat: "resource://normandy/lib/Heartbeat.sys.mjs",
  NormandyUtils: "resource://normandy/lib/NormandyUtils.sys.mjs",
  ProfilesDatastoreService:
    "moz-src:///toolkit/profile/ProfilesDatastoreService.sys.mjs",
  ShellService: "resource:///modules/ShellService.sys.mjs",
  UpdateUtils: "resource://gre/modules/UpdateUtils.sys.mjs",
});

const DAY_IN_MS = 24 * 60 * 60 * 1000;
const HEARTBEAT_THROTTLE = 1 * DAY_IN_MS;

export class ShowHeartbeatAction extends BaseAction {
  static Heartbeat = lazy.Heartbeat;

  static overrideHeartbeatForTests(newHeartbeat) {
    if (newHeartbeat) {
      this.Heartbeat = newHeartbeat;
    } else {
      this.Heartbeat = lazy.Heartbeat;
    }
  }

  get schema() {
    return lazy.ActionSchemas["show-heartbeat"];
  }

  async _run(recipe) {
    const {
      message,
      engagementButtonLabel,
      thanksMessage,
      learnMoreMessage,
      learnMoreUrl,
    } = recipe.arguments;

    if (!(await this.shouldShow(recipe))) {
      return;
    }

    this.log.debug(
      `Heartbeat for recipe ${recipe.id} showing prompt "${message}"`
    );
    const targetWindow = lazy.BrowserWindowTracker.getTopWindow();

    if (!targetWindow) {
      throw new Error("No window to show heartbeat in");
    }

    const heartbeat = new ShowHeartbeatAction.Heartbeat(targetWindow, {
      surveyId: this.generateSurveyId(recipe),
      message,
      engagementButtonLabel,
      thanksMessage,
      learnMoreMessage,
      learnMoreUrl,
      postAnswerUrl: await this.generatePostAnswerURL(recipe),
      flowId: lazy.NormandyUtils.generateUuid(),
      surveyVersion: recipe.revision_id,
    });

    heartbeat.eventEmitter.once(
      "Voted",
      this.updateLastInteraction.bind(this, recipe.id)
    );
    heartbeat.eventEmitter.once(
      "Engaged",
      this.updateLastInteraction.bind(this, recipe.id)
    );
  }

  async shouldShow(recipe) {
    const { repeatOption, repeatEvery } = recipe.arguments;
    // Don't show any heartbeats to a user more than once per throttle period
    let lastShown = await ShowHeartbeatAction._getLastShown();
    if (lastShown) {
      const duration = new Date() - lastShown;
      if (duration < HEARTBEAT_THROTTLE) {
        // show the number of hours since the last heartbeat, with at most 1 decimal point.
        const hoursAgo = Math.floor(duration / 1000 / 60 / 6) / 10;
        this.log.debug(
          `A heartbeat was shown too recently (${hoursAgo} hours), skipping recipe ${recipe.id}.`
        );
        return false;
      }
    }

    switch (repeatOption) {
      case "once": {
        // Don't show if we've ever shown before
        if (await ShowHeartbeatAction._getLastShown(recipe.id)) {
          this.log.debug(
            `Heartbeat for "once" recipe ${recipe.id} has been shown before, skipping.`
          );
          return false;
        }
        break;
      }

      case "nag": {
        // TODO(bug 1967853): nag was removed from LegacyHeartbeat.schema.json and
        // the logic for it needs to be updated, because it could result in multiple
        // profiles showing the same nag message.
        return false;
      }

      case "xdays": {
        // Show this heartbeat again if it has been at least `repeatEvery` days since the last time it was shown.
        if (lastShown) {
          const duration = new Date() - lastShown;
          if (duration < repeatEvery * DAY_IN_MS) {
            // show the number of hours since the last time this hearbeat was shown, with at most 1 decimal point.
            const hoursAgo = Math.floor(duration / 1000 / 60 / 6) / 10;
            this.log.debug(
              `Heartbeat for "xdays" recipe ${recipe.id} ran in the last ${repeatEvery} days, skipping. (${hoursAgo} hours ago)`
            );
            return false;
          }
        }
      }
    }

    let now = Date.now();
    await ShowHeartbeatAction._setLastShown(recipe.id, now);

    return true;
  }

  /**
   * Returns a surveyId value. If recipe calls to include the Normandy client
   * ID, then the client ID is attached to the surveyId in the format
   * `${surveyId}::${userId}`.
   *
   * @return {String} Survey ID, possibly with user UUID
   */
  generateSurveyId(recipe) {
    const { includeTelemetryUUID, surveyId } = recipe.arguments;
    if (includeTelemetryUUID) {
      return `${surveyId}::${lazy.ClientEnvironment.userId}`;
    }
    return surveyId;
  }

  /**
   * Generate the appropriate post-answer URL for a recipe.
   * @param  recipe
   * @return {String} URL with post-answer query params
   */
  async generatePostAnswerURL(recipe) {
    const { postAnswerUrl, message, includeTelemetryUUID } = recipe.arguments;

    // Don`t bother with empty URLs.
    if (!postAnswerUrl) {
      return postAnswerUrl;
    }

    const userId = lazy.ClientEnvironment.userId;
    const searchEngine = (await Services.search.getDefault()).identifier;
    const args = {
      fxVersion: Services.appinfo.version,
      isDefaultBrowser: lazy.ShellService.isDefaultBrowser() ? 1 : 0,
      searchEngine,
      source: "heartbeat",
      // `surveyversion` used to be the version of the heartbeat action when it
      // was hosted on a server. Keeping it around for compatibility.
      surveyversion: Services.appinfo.version,
      syncSetup: Services.prefs.prefHasUserValue("services.sync.username")
        ? 1
        : 0,
      updateChannel: lazy.UpdateUtils.getUpdateChannel(false),
      utm_campaign: encodeURIComponent(message.replace(/\s+/g, "")),
      utm_medium: recipe.action,
      utm_source: "firefox",
    };
    if (includeTelemetryUUID) {
      args.userId = userId;
    }

    let url = new URL(postAnswerUrl);
    // create a URL object to append arguments to
    for (const [key, val] of Object.entries(args)) {
      if (!url.searchParams.has(key)) {
        url.searchParams.set(key, val);
      }
    }

    // return the address with encoded queries
    return url.toString();
  }

  updateLastInteraction(recipeId) {
    ShowHeartbeatAction._setLastInteraction(recipeId, Date.now());
  }

  /**
   * Get last shown time in milliseconds since epoch for a recipe.
   * @param {string | null} recipeId
   *        ID of the recipe to look up, or null for the max across recipes.
   * @returns {Promise<number | null>} The last shown date, if any.
   */
  static async _getLastShown(recipeId = null) {
    const conn = await lazy.ProfilesDatastoreService.getConnection();
    let rows;
    if (recipeId === null) {
      rows = await conn.executeCached(
        "SELECT MAX(lastShown) AS lastShown FROM Heartbeats;"
      );
    } else {
      rows = await conn.executeCached(
        "SELECT lastShown FROM Heartbeats WHERE recipeId = :recipeId;",
        { recipeId }
      );
    }
    return rows[0]?.getResultByName("lastShown") ?? null;
  }

  /**
   * Get last interaction time in milliseconds since epoch for a recipe.
   * @param {string | null} recipeId
   *        ID of the recipe to look up, or null for the max across recipes.
   * @returns {Promise<number | null>} The last interaction date, if any.
   */
  static async _getLastInteraction(recipeId = null) {
    const conn = await lazy.ProfilesDatastoreService.getConnection();
    let rows;
    if (recipeId === null) {
      rows = await conn.executeCached(
        "SELECT MAX(lastInteraction) AS lastInteraction FROM Heartbeats;"
      );
    } else {
      rows = await conn.executeCached(
        "SELECT lastInteraction FROM Heartbeats WHERE recipeId = :recipeId;",
        { recipeId }
      );
    }
    return rows[0]?.getResultByName("lastInteraction") ?? null;
  }

  /**
   * Set a last shown for a recipe.
   * @param {string} recipeId
   *        ID of the recipe to update.
   * @param {number} lastShown
   *        Time the recipe was last shown, in milliseconds since epoch, as
   *        given by Date.now().
   */
  static async _setLastShown(recipeId, lastShown) {
    const conn = await lazy.ProfilesDatastoreService.getConnection();
    await conn.executeCached(
      `
          INSERT INTO Heartbeats(recipeId, lastShown)
          VALUES (:recipeId, :lastShown)
          ON CONFLICT(recipeId) DO UPDATE
          SET lastShown = :lastShown;`,
      { recipeId, lastShown }
    );
  }

  /**
   * Set a last interaction for a recipe.
   * @param {string} recipeId
   *        ID of the recipe to update.
   * @param {number} lastInteraction
   *        Time the recipe was last interacted with, in milliseconds since
   *        epoch, as given by Date.now().
   */
  static async _setLastInteraction(recipeId, lastInteraction) {
    const conn = await lazy.ProfilesDatastoreService.getConnection();
    await conn.executeCached(
      `
          INSERT INTO Heartbeats(recipeId, lastInteraction)
          VALUES (:recipeId, :lastInteraction)
          ON CONFLICT(recipeId) DO UPDATE
          SET lastInteraction = :lastInteraction;`,
      { recipeId, lastInteraction }
    );
  }

  /**
   * Clear ALL storage data. Only for use in tests.
   */
  static async _clearAllStorage() {
    const conn = await lazy.ProfilesDatastoreService.getConnection();
    await conn.executeCached("DELETE FROM Heartbeats;");
  }
}
