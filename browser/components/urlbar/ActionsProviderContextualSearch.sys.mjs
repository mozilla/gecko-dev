/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { UrlbarUtils } from "resource:///modules/UrlbarUtils.sys.mjs";

import {
  ActionsProvider,
  ActionsResult,
} from "resource:///modules/ActionsProvider.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  OpenSearchEngine: "resource://gre/modules/OpenSearchEngine.sys.mjs",
  loadAndParseOpenSearchEngine:
    "resource://gre/modules/OpenSearchLoader.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderAutofill: "resource:///modules/UrlbarProviderAutofill.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
});

const ENABLED_PREF = "contextualSearch.enabled";

const INSTALLED_ENGINE = "installed-engine";
const OPEN_SEARCH_ENGINE = "opensearch-engine";
const CONTEXTUAL_SEARCH_ENGINE = "contextual-search-engine";

const DEFAULT_ICON = "chrome://browser/skin/search-engine-placeholder@2x.png";

/**
 * A provider that returns an option for using the search engine provided
 * by the active view if it utilizes OpenSearch.
 */
class ProviderContextualSearch extends ActionsProvider {
  // Cache the results of engines looked up by host, these can be
  // expensive lookups and we don't want to redo the query every time
  // the user types when the result will not change.
  #hostEngines = new Map();
  // Store the engine returned to the user in case they select it.
  #resultEngine = null;

  constructor() {
    super();
  }

  get name() {
    return "ActionsProviderContextualSearch";
  }

  isActive(queryContext) {
    return (
      queryContext.trimmedSearchString &&
      lazy.UrlbarPrefs.getScotchBonnetPref(ENABLED_PREF) &&
      !queryContext.searchMode &&
      lazy.UrlbarPrefs.get("suggest.engines")
    );
  }

  async queryActions(queryContext) {
    this.#resultEngine = await this.matchEngine(queryContext);
    let defaultEngine = lazy.UrlbarSearchUtils.getDefaultEngine();

    if (
      this.#resultEngine &&
      this.#resultEngine.engine?.name != defaultEngine?.name
    ) {
      return [await this.createActionResult(this.#resultEngine)];
    }
    return null;
  }

  onSearchSessionEnd() {
    // We cache the results for a host while the user is typing, clear
    // when the search session ends as the results for the host may
    // change by the next search session.
    this.#hostEngines.clear();
  }

  async createActionResult({ type, engine }) {
    let icon = engine?.icon || (await engine?.getIconURL?.()) || DEFAULT_ICON;
    return new ActionsResult({
      key: "contextual-search",
      l10nId: "urlbar-result-search-with",
      l10nArgs: { engine: engine.name || engine.title },
      icon,
      onPick: (context, controller) => {
        this.pickAction(context, controller);
      },
      onSelection: async (result, element) => {
        // We don't enter preview searchMode unless the engine is installed.
        if (type != INSTALLED_ENGINE) {
          return;
        }
        result.payload.engine = engine.name;
        result.payload.query = "";
        element.ownerGlobal.gURLBar.maybeConfirmSearchModeFromResult({
          result,
          checkValue: false,
          startQuery: false,
        });
      },
    });
  }

  /*
   * Searches for engines that we want to present to the user based on their
   * current host and the search query they have entered.
   */
  async matchEngine(queryContext) {
    // First find currently installed engines that match the current query
    // if the user has DuckDuckGo installed and types "duck", offer that.
    let engine = await this.#matchTabToSearchEngine(queryContext);
    if (engine) {
      return engine;
    }

    let browser =
      lazy.BrowserWindowTracker.getTopWindow().gBrowser.selectedBrowser;
    let host;
    try {
      host = UrlbarUtils.stripPrefixAndTrim(browser.currentURI.host, {
        stripWww: true,
      })[0];
    } catch (e) {
      // about: pages will throw when access currentURI.host, ignore.
    }

    // Find engines based on the current host.
    if (host && !this.#hostEngines.has(host)) {
      // Find currently installed engines that match the current host. If
      // the user is on wikipedia.com, offer that.
      let hostEngine = await this.#matchInstalledEngine(host);

      if (!hostEngine) {
        // Find engines in the search configuration but not installed that match
        // the current host. If the user is on ecosia.com and starts searching
        // offer ecosia's search.
        let contextualEngineConfig =
          await Services.search.findContextualSearchEngineByHost(host);
        if (contextualEngineConfig) {
          hostEngine = {
            type: CONTEXTUAL_SEARCH_ENGINE,
            engine: contextualEngineConfig,
          };
        }
      }
      // Cache the result against this host so we do not need to rerun
      // the same query every keystroke.
      this.#hostEngines.set(host, hostEngine);
      if (hostEngine) {
        return hostEngine;
      }
    } else if (host) {
      let cachedEngine = this.#hostEngines.get(host);
      if (cachedEngine) {
        return cachedEngine;
      }
    }

    // Lastly match any openSearch
    if (browser?.engines?.length) {
      return { type: OPEN_SEARCH_ENGINE, engine: browser.engines[0] };
    }

    return null;
  }

  async #matchInstalledEngine(query) {
    let engines = await lazy.UrlbarSearchUtils.enginesForDomainPrefix(query, {
      matchAllDomainLevels: true,
    });
    if (engines.length) {
      return { type: INSTALLED_ENGINE, engine: engines[0] };
    }
    return null;
  }

  /*
   * This logic is copied from `UrlbarProviderTabToSearch.sys.mjs` and
   * matches a users search query to an installed engine.
   */
  async #matchTabToSearchEngine(queryContext) {
    let searchStr = queryContext.trimmedSearchString.toLocaleLowerCase();
    let engines = await lazy.UrlbarSearchUtils.enginesForDomainPrefix(
      searchStr,
      {
        matchAllDomainLevels: true,
      }
    );

    if (!engines.length) {
      return null;
    }

    let partialMatchEnginesByHost = new Map();

    for (let engine of engines) {
      let [host] = UrlbarUtils.stripPrefixAndTrim(engine.searchUrlDomain, {
        stripWww: true,
      });
      if (host.startsWith(searchStr)) {
        return { type: INSTALLED_ENGINE, engine };
      }
      if (host.includes("." + searchStr)) {
        partialMatchEnginesByHost.set(engine.searchUrlDomain, engine);
      }
      let baseDomain = Services.eTLD.getBaseDomainFromHost(
        engine.searchUrlDomain
      );
      if (baseDomain.startsWith(searchStr)) {
        partialMatchEnginesByHost.set(baseDomain, engine);
      }
    }

    if (partialMatchEnginesByHost.size) {
      let host = await lazy.UrlbarProviderAutofill.getTopHostOverThreshold(
        queryContext,
        Array.from(partialMatchEnginesByHost.keys())
      );
      if (host) {
        let engine = partialMatchEnginesByHost.get(host);
        return { type: INSTALLED_ENGINE, engine };
      }
    }
    return null;
  }

  async pickAction(queryContext, controller, _element) {
    let { type, engine } = this.#resultEngine;

    if (type == OPEN_SEARCH_ENGINE) {
      let openSearchEngineData = await lazy.loadAndParseOpenSearchEngine(
        Services.io.newURI(engine.uri)
      );
      engine = new lazy.OpenSearchEngine({ engineData: openSearchEngineData });
    }

    this.#performSearch(
      engine,
      queryContext.searchString,
      controller.input,
      type == INSTALLED_ENGINE
    );

    if (
      !queryContext.isPrivate &&
      type != INSTALLED_ENGINE &&
      (await Services.search.shouldShowInstallPrompt(engine))
    ) {
      this.#showInstallPrompt(controller, engine);
    }
  }

  async #performSearch(engine, search, input, enterSearchMode) {
    const [url] = UrlbarUtils.getSearchQueryUrl(engine, search);
    if (enterSearchMode) {
      input.search(search, { searchEngine: engine });
    }
    input.window.gBrowser.fixupAndLoadURIString(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    input.window.gBrowser.selectedBrowser.focus();
  }

  #showInstallPrompt(controller, engineData) {
    let win = controller.input.window;
    let buttons = [
      {
        "l10n-id": "install-search-engine-add",
        callback() {
          Services.search.addSearchEngine(engineData);
        },
      },
      {
        "l10n-id": "install-search-engine-no",
        callback() {},
      },
    ];

    win.gNotificationBox.appendNotification(
      "install-search-engine",
      {
        label: {
          "l10n-id": "install-search-engine",
          "l10n-args": { engineName: engineData.name },
        },
        image: "chrome://global/skin/icons/question-64.png",
        priority: win.gNotificationBox.PRIORITY_INFO_LOW,
      },
      buttons
    );
  }
}

export var ActionsProviderContextualSearch = new ProviderContextualSearch();
