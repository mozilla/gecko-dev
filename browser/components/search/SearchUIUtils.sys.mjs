/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Various utilities for search related UI.
 */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "SearchUIUtilsL10n", () => {
  return new Localization(["browser/search.ftl", "branding/brand.ftl"]);
});

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserSearchTelemetry: "resource:///modules/BrowserSearchTelemetry.sys.mjs",
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  CustomizableUI: "resource:///modules/CustomizableUI.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  URILoadingHelper: "resource:///modules/URILoadingHelper.sys.mjs",
});

export var SearchUIUtils = {
  initialized: false,

  init() {
    if (!this.initialized) {
      Services.obs.addObserver(this, "browser-search-engine-modified");

      this.initialized = true;
    }
  },

  observe(engine, topic, data) {
    switch (data) {
      case "engine-default":
        this.updatePlaceholderNamePreference(engine, false);
        break;
      case "engine-default-private":
        this.updatePlaceholderNamePreference(engine, true);
        break;
    }
  },

  /**
   * Adds an open search engine and handles error UI.
   *
   * @param {string} locationURL
   *   The URL where the OpenSearch definition is located.
   * @param {string} image
   *   A URL string to an icon file to be used as the search engine's
   *   icon. This value may be overridden by an icon specified in the
   *   engine description file.
   * @param {object} browsingContext
   *   The browsing context any error prompt should be opened for.
   * @returns {boolean}
   *   Returns true if the engine was added.
   */
  async addOpenSearchEngine(locationURL, image, browsingContext) {
    try {
      await Services.search.addOpenSearchEngine(locationURL, image);
    } catch (ex) {
      let titleMsgName;
      let descMsgName;
      switch (ex.result) {
        case Ci.nsISearchService.ERROR_DUPLICATE_ENGINE:
          titleMsgName = "opensearch-error-duplicate-title";
          descMsgName = "opensearch-error-duplicate-desc";
          break;
        case Ci.nsISearchService.ERROR_ENGINE_CORRUPTED:
          titleMsgName = "opensearch-error-format-title";
          descMsgName = "opensearch-error-format-desc";
          break;
        default:
          // i.e. ERROR_DOWNLOAD_FAILURE
          titleMsgName = "opensearch-error-download-title";
          descMsgName = "opensearch-error-download-desc";
          break;
      }

      let [title, text] = await lazy.SearchUIUtilsL10n.formatValues([
        {
          id: titleMsgName,
        },
        {
          id: descMsgName,
          args: {
            "location-url": locationURL,
          },
        },
      ]);

      Services.prompt.alertBC(
        browsingContext,
        Ci.nsIPrompt.MODAL_TYPE_CONTENT,
        title,
        text
      );
      return false;
    }
    return true;
  },

  /**
   * Returns the URL to use for where to get more search engines.
   *
   * @returns {string}
   */
  get searchEnginesURL() {
    return Services.urlFormatter.formatURLPref(
      "browser.search.searchEnginesURL"
    );
  },

  /**
   * Update the placeholderName preference for the default search engine.
   *
   * @param {nsISearchEngine} engine The new default search engine.
   * @param {boolean} isPrivate Whether this change applies to private windows.
   */
  updatePlaceholderNamePreference(engine, isPrivate) {
    const prefName =
      "browser.urlbar.placeholderName" + (isPrivate ? ".private" : "");
    if (engine.isAppProvided) {
      Services.prefs.setStringPref(prefName, engine.name);
    } else {
      Services.prefs.clearUserPref(prefName);
    }
  },

  /**
   * Focuses the search bar if present on the toolbar, or the address bar,
   * putting it in search mode. Will do so in an existing non-popup browser
   * window or open a new one if necessary.
   *
   * @param {WindowProxy} window
   *   The window where the seach was triggered.
   */
  webSearch(window) {
    if (
      window.location.href != AppConstants.BROWSER_CHROME_URL ||
      window.gURLBar.readOnly
    ) {
      let topWindow = lazy.URILoadingHelper.getTopWin(window, {
        skipPopups: true,
      });
      if (topWindow) {
        // If there's an open browser window, it should handle this command
        topWindow.focus();
        SearchUIUtils.webSearch(topWindow);
      } else {
        // If there are no open browser windows, open a new one
        let newWindow = window.openDialog(
          AppConstants.BROWSER_CHROME_URL,
          "_blank",
          "chrome,all,dialog=no",
          "about:blank"
        );

        let observer = subject => {
          if (subject == newWindow) {
            SearchUIUtils.webSearch(newWindow);
            Services.obs.removeObserver(
              observer,
              "browser-delayed-startup-finished"
            );
          }
        };
        Services.obs.addObserver(observer, "browser-delayed-startup-finished");
      }
      return;
    }

    let focusUrlBarIfSearchFieldIsNotActive = function (aSearchBar) {
      if (!aSearchBar || window.document.activeElement != aSearchBar.textbox) {
        // Limit the results to search suggestions, like the search bar.
        window.gURLBar.searchModeShortcut();
      }
    };

    let searchBar = window.BrowserSearch.searchBar;
    let placement =
      lazy.CustomizableUI.getPlacementOfWidget("search-container");
    let focusSearchBar = () => {
      searchBar = window.BrowserSearch.searchBar;
      searchBar.select();
      focusUrlBarIfSearchFieldIsNotActive(searchBar);
    };
    if (
      placement &&
      searchBar &&
      ((searchBar.parentNode.getAttribute("overflowedItem") == "true" &&
        placement.area == lazy.CustomizableUI.AREA_NAVBAR) ||
        placement.area == lazy.CustomizableUI.AREA_FIXED_OVERFLOW_PANEL)
    ) {
      let navBar = window.document.getElementById(
        lazy.CustomizableUI.AREA_NAVBAR
      );
      navBar.overflowable.show().then(focusSearchBar);
      return;
    }
    if (searchBar) {
      if (window.fullScreen) {
        window.FullScreen.showNavToolbox();
      }
      searchBar.select();
    }
    focusUrlBarIfSearchFieldIsNotActive(searchBar);
  },

  /**
   * Loads a search results page, given a set of search terms. Uses the current
   * engine if the search bar is visible, or the default engine otherwise.
   *
   * @param {WindowProxy} window
   *   The window where the search was triggered.
   * @param {string} searchText
   *   The search terms to use for the search.
   * @param {?string} where
   *   String indicating where the search should load. Most commonly used
   *   are 'tab' or 'window', defaults to 'current'.
   * @param {boolean} usePrivate
   *   Whether to use the Private Browsing mode default search engine.
   *   Defaults to `false`.
   * @param {nsIPrincipal} triggeringPrincipal
   *   The principal to use for a new window or tab.
   * @param {nsIContentSecurityPolicy} csp
   *   The content security policy to use for a new window or tab.
   * @param {boolean} [inBackground=false]
   *   Set to true for the tab to be loaded in the background.
   * @param {?nsISearchEngine} [engine=null]
   *   The search engine to use for the search.
   * @param {?NativeTab} [tab=null]
   *   The tab to show the search result.
   *
   * @returns {Promise<?{engine: nsISearchEngine, url: nsIURI}>}
   *   Object containing the search engine used to perform the
   *   search and the url, or null if no search was performed.
   */
  async _loadSearch(
    window,
    searchText,
    where,
    usePrivate,
    triggeringPrincipal,
    csp,
    inBackground = false,
    engine = null,
    tab = null
  ) {
    if (!triggeringPrincipal) {
      throw new Error(
        "Required argument triggeringPrincipal missing within _loadSearch"
      );
    }

    if (!engine) {
      engine = usePrivate
        ? await Services.search.getDefaultPrivate()
        : await Services.search.getDefault();
    }

    let submission = engine.getSubmission(searchText);

    // getSubmission can return null if the engine doesn't have a URL
    // with a text/html response type. This is unlikely (since
    // SearchService._addEngineToStore() should fail for such an engine),
    // but let's be on the safe side.
    if (!submission) {
      return null;
    }

    window.openLinkIn(submission.uri.spec, where || "current", {
      private: usePrivate && !lazy.PrivateBrowsingUtils.isWindowPrivate(window),
      postData: submission.postData,
      inBackground,
      relatedToCurrent: true,
      triggeringPrincipal,
      csp,
      targetBrowser: tab?.linkedBrowser,
      globalHistoryOptions: {
        triggeringSearchEngine: engine.name,
      },
    });

    return { engine, url: submission.uri };
  },

  /**
   * Perform a search initiated from the context menu.
   * This should only be called from the context menu.
   *
   * @param {WindowProxy} window
   *   The window where the search was triggered.
   * @param {string} searchText
   *   The search terms to use for the search.
   * @param {boolean} usePrivate
   *   Whether to use the Private Browsing mode default search engine.
   *   Defaults to `false`.
   * @param {nsIPrincipal} triggeringPrincipal
   *   The principal of the document whose context menu was clicked.
   * @param {nsIContentSecurityPolicy} csp
   *   The content security policy to use for a new window or tab.
   * @param {Event} event
   *   The event triggering the search.
   */
  async loadSearchFromContext(
    window,
    searchText,
    usePrivate,
    triggeringPrincipal,
    csp,
    event
  ) {
    event = lazy.BrowserUtils.getRootEvent(event);
    let where = lazy.BrowserUtils.whereToOpenLink(event);
    if (where == "current") {
      // override: historically search opens in new tab
      where = "tab";
    }
    if (usePrivate && !lazy.PrivateBrowsingUtils.isWindowPrivate(window)) {
      where = "window";
    }
    let inBackground = Services.prefs.getBoolPref(
      "browser.search.context.loadInBackground"
    );
    if (event.button == 1 || event.ctrlKey) {
      inBackground = !inBackground;
    }

    let searchInfo = await SearchUIUtils._loadSearch(
      window,
      searchText,
      where,
      usePrivate,
      Services.scriptSecurityManager.createNullPrincipal(
        triggeringPrincipal.originAttributes
      ),
      csp,
      inBackground
    );

    if (searchInfo) {
      lazy.BrowserSearchTelemetry.recordSearch(
        window.gBrowser.selectedBrowser,
        searchInfo.engine,
        "contextmenu"
      );
    }
  },

  /**
   * Perform a search initiated from the command line.
   *
   * @param {WindowProxy} window
   *   The window where the search was triggered.
   * @param {string} searchText
   *   The search terms to use for the search.
   * @param {boolean} usePrivate
   *   Whether to use the Private Browsing mode default search engine.
   *   Defaults to `false`.
   * @param {nsIPrincipal} triggeringPrincipal
   *   The principal to use for a new window or tab.
   * @param {nsIContentSecurityPolicy} csp
   *   The content security policy to use for a new window or tab.
   */
  async loadSearchFromCommandLine(
    window,
    searchText,
    usePrivate,
    triggeringPrincipal,
    csp
  ) {
    let searchInfo = await SearchUIUtils._loadSearch(
      window,
      searchText,
      "current",
      usePrivate,
      triggeringPrincipal,
      csp
    );
    if (searchInfo) {
      lazy.BrowserSearchTelemetry.recordSearch(
        window.gBrowser.selectedBrowser,
        searchInfo.engine,
        "system"
      );
    }
  },

  /**
   * Perform a search initiated from an extension.
   *
   * @param {object} params
   *   The params.
   * @param {WindowProxy} params.window
   *   The window where the search was triggered.
   * @param {string} params.query
   *   The search terms to use for the search.
   * @param {nsISearchEngine} params.engine
   *   The search engine to use for the search.
   * @param {string} params.where
   *   String indicating where the search should load.
   * @param {NativeTab} params.tab
   *   The tab to show the search result.
   * @param {nsIPrincipal} params.triggeringPrincipal
   *   The principal to use for a new window or tab.
   */
  async loadSearchFromExtension({
    window,
    query,
    engine,
    where,
    tab,
    triggeringPrincipal,
  }) {
    let searchInfo = await SearchUIUtils._loadSearch(
      window,
      query,
      where,
      lazy.PrivateBrowsingUtils.isWindowPrivate(window),
      triggeringPrincipal,
      null,
      false,
      engine,
      tab
    );

    if (searchInfo) {
      lazy.BrowserSearchTelemetry.recordSearch(
        window.gBrowser.selectedBrowser,
        searchInfo.engine,
        "webextension"
      );
    }
  },
};
