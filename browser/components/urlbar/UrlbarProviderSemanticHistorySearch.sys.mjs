/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a provider that offers search history suggestions
 * based on embeddings and semantic search techniques using semantic
 * history
 */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";
import { PlacesSemanticHistoryManager } from "resource://gre/modules/PlacesSemanticHistoryManager.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

/**
 * Class representing the Semantic History Search provider for the URL bar.
 *
 * This provider queries a semantic database created using history.
 * It performs semantic search using embeddings generated
 * by an ML model and retrieves results ranked by cosine similarity to the
 * query's embedding.
 *
 * @class
 */
class ProviderSemanticHistorySearch extends UrlbarProvider {
  #semanticManager;

  /**
   * Lazily creates (on first call) and returns the
   * {@link PlacesSemanticHistoryManager} instance backing this provider.
   *
   * The manager is instantiated only once and cached in the private
   * `#semanticManager` field.  It is configured with sensible defaults for
   * semantic history search:
   *   • `embeddingSize`: 384 – dimensionality of vector embeddings
   *   • `rowLimit`: 10000 – maximum rows pulled from Places
   *   • `samplingAttrib`: "frecency" – column used when down-sampling
   *   • `changeThresholdCount`: 3 – restart inference after this many DB changes
   *   • `distanceThreshold`: 0.75 – cosine-distance cut-off for matches
   *
   * @returns {PlacesSemanticHistoryManager}
   *   The shared, initialized semantic-history manager instance.
   */
  ensureSemanticManagerInitialized() {
    if (!this.#semanticManager) {
      this.#semanticManager = new PlacesSemanticHistoryManager({
        embeddingSize: 384,
        rowLimit: 10000,
        samplingAttrib: "frecency",
        changeThresholdCount: 3,
        distanceThreshold: 0.75,
      });
    }
    return this.#semanticManager;
  }

  get name() {
    return "SemanticHistorySearch";
  }

  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  /**
   * Determines if the provider is active for the given query context.
   *
   * @param {object} queryContext
   *   The context of the query, including the search string.
   * @returns {boolean}
   *   `true` if the provider is active; `false` otherwise.
   */
  isActive(queryContext) {
    const semanticHistoryFlag = lazy.UrlbarPrefs.get("suggest.semanticHistory");
    const minSearchStringLength = lazy.UrlbarPrefs.get(
      "suggest.semanticHistory.minLength"
    );
    if (
      semanticHistoryFlag &&
      queryContext.searchString.length >= minSearchStringLength
    ) {
      const semanticManager = this.ensureSemanticManagerInitialized();
      return semanticManager?.canUseSemanticSearch() ?? false;
    }
    return false;
  }

  /**
   * Starts a semantic search query.
   *
   * @param {object} queryContext
   *   The query context, including the search string.
   * @param {Function} addCallback
   *   Callback to add results to the URL bar.
   */
  async startQuery(queryContext, addCallback) {
    let instance = this.queryInstance;
    if (!this.#semanticManager) {
      throw new Error(
        "SemanticManager must be initialized via isActive() before calling startQuery()"
      );
    }

    let resultObject = await this.#semanticManager.infer(queryContext);
    let results = resultObject.results;
    if (!results || instance != this.queryInstance) {
      return;
    }
    for (let res of results) {
      const result = new lazy.UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        {
          title: res.title,
          url: res.url,
          icon: UrlbarUtils.getIconForUrl(res.url),
        }
      );
      result.resultGroup = UrlbarUtils.RESULT_GROUP.HISTORY_SEMANTIC;
      addCallback(this, result);
    }
  }

  /**
   * Gets the priority of this provider relative to other providers.
   *
   * @returns {number} The priority of this provider.
   */
  getPriority() {
    return 0;
  }
}

export var UrlbarProviderSemanticHistorySearch =
  new ProviderSemanticHistorySearch();
