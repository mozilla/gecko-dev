/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
});

/**
 * Manages the set of available opensearch engines per browser.
 */
class _OpenSearchManager {
  /**
   * @typedef {object} OpenSearchData
   * @property {string} uri
   *   The uri of the opensearch XML.
   * @property {string} title
   *   The name of the engine.
   * @property {string} icon
   *   Data URI containing the engine's icon.
   */

  /**
   * @type {WeakMap<XULBrowserElement, OpenSearchData[]>}
   */
  #offeredEngines = new WeakMap();

  /**
   * @type {WeakMap<XULBrowserElement, OpenSearchData[]>}
   */
  #hiddenEngines = new WeakMap();

  constructor() {
    Services.obs.addObserver(this, "browser-search-engine-modified");
  }

  /**
   * Observer for browser-search-engine-modified.
   *
   * @param {nsISearchEngine} engine
   *   The modified engine.
   * @param {string} _topic
   *   Always browser-search-engine-modified.
   * @param {string} data
   *   The type of modification.
   */
  observe(engine, _topic, data) {
    // There are two kinds of search engine objects: nsISearchEngine objects
    // and plain OpenSearchData objects. `engine` in this observer is the
    // former and the arrays in #offeredEngines and #hiddenEngines contain the
    // latter. They are related by their names.
    switch (data) {
      case "engine-added":
        // An engine was added to the search service.  If a page is offering the
        // engine, then the engine needs to be removed from the corresponding
        // browser's offered engines.
        this.#removeMaybeOfferedEngine(engine.name);
        break;
      case "engine-removed":
        // An engine was removed from the search service.  If a page is offering
        // the engine, then the engine needs to be added back to the corresponding
        // browser's offered engines.
        this.#addMaybeOfferedEngine(engine.name);
        break;
    }
  }

  /**
   * Adds an open search engine to the list of available engines for a browser.
   * If an engine with that name is already installed, adds it to the list
   * of hidden engines instead.
   *
   * @param {XULBrowserElement} browser
   *   The browser offering the engine.
   * @param {{title: string, href: string}} engine
   *   The title of the engine and the url to the opensearch XML.
   */
  addEngine(browser, engine) {
    if (!Services.search.hasSuccessfullyInitialized) {
      // We haven't finished initializing search yet. This means we can't
      // call getEngineByName here. Since this is only on start-up and unlikely
      // to happen in the normal case, we'll just return early rather than
      // trying to handle it asynchronously.
      return;
    }
    // Check to see whether we've already added an engine with this title
    if (this.#offeredEngines.get(browser)?.some(e => e.title == engine.title)) {
      return;
    }

    // If this engine (identified by title) is already in the list, add it
    // to the list of hidden engines rather than to the main list.
    let shouldBeHidden = !!Services.search.getEngineByName(engine.title);

    let engines =
      (shouldBeHidden
        ? this.#hiddenEngines.get(browser)
        : this.#offeredEngines.get(browser)) || [];

    engines.push({
      uri: engine.href,
      title: engine.title,
      get icon() {
        return browser.mIconURL;
      },
    });

    if (shouldBeHidden) {
      this.#hiddenEngines.set(browser, engines);
    } else {
      let win = browser.ownerGlobal;
      this.#offeredEngines.set(browser, engines);
      if (browser == win.gBrowser.selectedBrowser) {
        this.updateOpenSearchBadge(win);
      }
    }
  }

  /**
   * Updates the browser UI to show whether or not additional engines are
   * available when a page is loaded or the user switches tabs to a page that
   * has open search engines.
   *
   * @param {WindowProxy} win
   *   The window whose UI should be updated.
   */
  updateOpenSearchBadge(win) {
    let engines = this.#offeredEngines.get(win.gBrowser.selectedBrowser);
    win.gURLBar.addSearchEngineHelper.setEnginesFromBrowser(
      win.gBrowser.selectedBrowser,
      engines || []
    );

    let searchBar = win.document.getElementById("searchbar");
    if (!searchBar) {
      return;
    }

    if (engines && engines.length) {
      searchBar.setAttribute("addengines", "true");
    } else {
      searchBar.removeAttribute("addengines");
    }
  }

  #addMaybeOfferedEngine(engineName) {
    for (let win of lazy.BrowserWindowTracker.orderedWindows) {
      for (let browser of win.gBrowser.browsers) {
        let hiddenEngines = this.#hiddenEngines.get(browser) || [];
        let offeredEngines = this.#offeredEngines.get(browser) || [];

        for (let i = 0; i < hiddenEngines.length; i++) {
          if (hiddenEngines[i].title == engineName) {
            offeredEngines.push(hiddenEngines[i]);
            if (offeredEngines.length == 1) {
              this.#offeredEngines.set(browser, offeredEngines);
            }

            hiddenEngines.splice(i, 1);
            if (browser == win.gBrowser.selectedBrowser) {
              this.updateOpenSearchBadge(win);
            }
            break;
          }
        }
      }
    }
  }

  #removeMaybeOfferedEngine(engineName) {
    for (let win of lazy.BrowserWindowTracker.orderedWindows) {
      for (let browser of win.gBrowser.browsers) {
        let hiddenEngines = this.#hiddenEngines.get(browser) || [];
        let offeredEngines = this.#offeredEngines.get(browser) || [];

        for (let i = 0; i < offeredEngines.length; i++) {
          if (offeredEngines[i].title == engineName) {
            hiddenEngines.push(offeredEngines[i]);
            if (hiddenEngines.length == 1) {
              this.#hiddenEngines.set(browser, hiddenEngines);
            }

            offeredEngines.splice(i, 1);
            if (browser == win.gBrowser.selectedBrowser) {
              this.updateOpenSearchBadge(win);
            }
            break;
          }
        }
      }
    }
  }

  /**
   * Get the open search engines offered by a certain browser.
   *
   * @param {XULBrowserElement} browser
   *   The browser for which to get the engines.
   * @returns {OpenSearchData[]}
   *   The open search engines.
   */
  getEngines(browser) {
    return this.#offeredEngines.get(browser) || [];
  }

  clearEngines(browser) {
    this.#offeredEngines.delete(browser);
    this.#hiddenEngines.delete(browser);
  }
}

export const OpenSearchManager = new _OpenSearchManager();
