/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SuggestBackend } from "resource:///modules/urlbar/private/SuggestFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  MerinoClient: "resource:///modules/MerinoClient.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

/**
 * The Suggest Merino backend. This backend is enabled when the user opts in to
 * Merino, also called "online" Suggest.
 */
export class SuggestBackendMerino extends SuggestBackend {
  get shouldEnable() {
    return lazy.UrlbarPrefs.get("quicksuggest.dataCollection.enabled");
  }

  get enablingPreferences() {
    return ["quicksuggest.dataCollection.enabled"];
  }

  /**
   * @returns {MerinoClient}
   *   The Merino client. The client is created lazily and isn't kept around
   *   when the backend is disabled, so this may return null.
   */
  get client() {
    return this.#client;
  }

  async enable(enabled) {
    if (!enabled) {
      this.#client = null;
    }
  }

  async query(searchString, { queryContext }) {
    if (!queryContext.allowRemoteResults()) {
      return [];
    }

    this.logger.debug("Handling query", { searchString });

    if (!this.#client) {
      this.#client = new lazy.MerinoClient(this.name);
    }

    let providers;
    if (
      !lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored") &&
      !lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored") &&
      !lazy.UrlbarPrefs.get("merinoProviders")
    ) {
      // Data collection is enabled but suggestions are not. Per product
      // requirements, we still want to ping Merino so it can record the query,
      // but pass an empty list of providers to tell it not to fetch any
      // suggestions.
      providers = [];
    }

    let suggestions = await this.#client.fetch({
      providers,
      query: searchString,
    });

    this.logger.debug("Got suggestions", suggestions);

    return suggestions;
  }

  cancelQuery() {
    // Cancel the Merino timeout timer so it doesn't fire and record a timeout.
    // If it's already canceled or has fired, this is a no-op.
    this.#client?.cancelTimeoutTimer();

    // Don't abort the Merino fetch if one is ongoing. By design we allow
    // fetches to finish so we can record their latency.
  }

  onSearchSessionEnd(_queryContext, _controller, _details) {
    // Reset the Merino session ID when a session ends. By design for the user's
    // privacy, we don't keep it around between engagements.
    this.#client?.resetSession();
  }

  // `MerinoClient`
  #client = null;
}
