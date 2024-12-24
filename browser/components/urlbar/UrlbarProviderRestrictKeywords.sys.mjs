/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a provider that offers restrict keywords for search mode.
 */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.sys.mjs",
});

const RESTRICT_KEYWORDS_FEATURE_GATE = "searchRestrictKeywords.featureGate";

/**
 * Class used to create the provider.
 */
class ProviderRestrictKeywords extends UrlbarProvider {
  constructor() {
    super();
  }

  get name() {
    return "RestrictKeywords";
  }

  get type() {
    return UrlbarUtils.PROVIDER_TYPE.HEURISTIC;
  }

  getPriority() {
    return 1;
  }

  isActive(queryContext) {
    if (!lazy.UrlbarPrefs.getScotchBonnetPref(RESTRICT_KEYWORDS_FEATURE_GATE)) {
      return false;
    }

    return !queryContext.searchMode && queryContext.trimmedSearchString == "@";
  }

  async startQuery(queryContext, addCallback) {
    let instance = this.queryInstance;
    let tokenToKeyword = await lazy.UrlbarTokenizer.getL10nRestrictKeywords();

    if (instance != this.queryInstance) {
      return;
    }

    for (const [token, l10nRestrictKeywords] of tokenToKeyword.entries()) {
      let icon = UrlbarUtils.LOCAL_SEARCH_MODES.find(
        mode => mode.restrict == token
      )?.icon;

      let result = new lazy.UrlbarResult(
        UrlbarUtils.RESULT_TYPE.RESTRICT,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
          icon,
          keyword: token,
          l10nRestrictKeywords: [
            l10nRestrictKeywords,
            UrlbarUtils.HIGHLIGHT.TYPED,
          ],
          providesSearchMode: true,
        })
      );
      addCallback(this, result);
    }
  }
}

export var UrlbarProviderRestrictKeywords = new ProviderRestrictKeywords();
