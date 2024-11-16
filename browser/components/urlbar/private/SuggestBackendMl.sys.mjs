/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  MLSuggest: "resource:///modules/urlbar/private/MLSuggest.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

/**
 * The Suggest ML backend. Both the ML and Rust backends can be enabled at the
 * same time. Features can support both backends and decide which one to use per
 * query.
 */
export class SuggestBackendMl extends BaseFeature {
  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("quickSuggestMlEnabled") &&
      lazy.UrlbarPrefs.get("browser.ml.enable")
    );
  }

  get enablingPreferences() {
    return ["browser.ml.enable"];
  }

  async enable(enabled) {
    if (enabled) {
      this.logger.debug("Initializing MLSuggest...");
      await lazy.MLSuggest.initialize();
      this.logger.debug("MLSuggest is now initialized");
    } else {
      this.logger.debug("Shutting down MLSuggest...");
      await lazy.MLSuggest.shutdown();
      this.logger.debug("MLSuggest is now shut down");
    }
  }

  /**
   * Queries `MLSuggest` and returns the matching suggestion.
   *
   * @param {string} searchString
   *   The search string.
   * @returns {Array}
   *   An array of matching suggestions. `MLSuggest` returns at most one
   *   suggestion.
   */
  async query(searchString) {
    this.logger.debug("Handling query: " + JSON.stringify(searchString));

    // Don't waste time calling into `MLSuggest` if no ML intents are enabled.
    if (
      lazy.QuickSuggest.mlFeatures
        .values()
        .every(f => !f.isEnabled || !f.isMlIntentEnabled)
    ) {
      this.logger.debug("No ML intents enabled, ignoring query");
      return [];
    }

    let suggestion = await lazy.MLSuggest.makeSuggestions(searchString);
    this.logger.debug("Got suggestion: " + JSON.stringify(suggestion, null, 2));

    if (suggestion) {
      // We can't force `MLSuggest` to return only enabled intents, so it can
      // return a disabled intent. Discard the suggestion in that case.
      let feature = lazy.QuickSuggest.getFeatureByMlIntent(suggestion.intent);
      if (!feature?.isEnabled || !feature?.isMlIntentEnabled) {
        this.logger.debug("No ML feature for suggestion, ignoring query");
        return [];
      }
      suggestion.source = "ml";
      suggestion.provider = suggestion.intent;
      return [suggestion];
    }

    return [];
  }
}
