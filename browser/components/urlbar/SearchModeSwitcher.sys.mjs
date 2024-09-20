/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PanelMultiView: "resource:///modules/PanelMultiView.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "SearchModeSwitcherL10n", () => {
  return new Localization(["preview/enUS-searchFeatures.ftl"]);
});

/**
 * Implements the SearchModeSwitcher in the urlbar.
 */
export class SearchModeSwitcher {
  #engineListNeedsRebuild = true;
  #popup;
  #input;
  #toolbarbutton;

  constructor(input) {
    this.#input = input;

    this.QueryInterface = ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]);

    lazy.UrlbarPrefs.addObserver(this);

    this.#popup = input.document.getElementById("searchmode-switcher-popup");

    this.#toolbarbutton = input.document.querySelector(
      "#urlbar-searchmode-switcher"
    );

    if (lazy.UrlbarPrefs.get("scotchBonnet.enableOverride")) {
      this.#enableObservers();
    }
  }

  /**
   * Open the SearchSwitcher popup.
   *
   * @param {Event} event
   *        The event that triggered the opening of the popup.
   */
  async openPanel(event) {
    if (
      (event.type == "click" && event.button != 0) ||
      (event.type == "keypress" &&
        event.charCode != KeyEvent.DOM_VK_SPACE &&
        event.keyCode != KeyEvent.DOM_VK_RETURN &&
        event.keyCode != KeyEvent.DOM_VK_DOWN)
    ) {
      return; // Left click, down arrow, space or enter only
    }

    let anchor = event.target;
    event.preventDefault();

    if (this.#input.document.documentElement.hasAttribute("customizing")) {
      return;
    }

    if (this.#engineListNeedsRebuild) {
      await this.#rebuildSearchModeList(this.#input.window);
      this.#engineListNeedsRebuild = false;
    }
    if (anchor.getAttribute("open") != "true") {
      this.#input.view.hideTemporarily();

      this.#popup.addEventListener(
        "popuphidden",
        () => {
          anchor.removeAttribute("open");
          anchor.setAttribute("aria-expanded", false);
          this.#input.view.restoreVisibility();
        },
        { once: true }
      );
      anchor.setAttribute("open", true);
      anchor.setAttribute("aria-expanded", true);

      if (event.type == "keypress") {
        // Focus the first item when opened by keypress only.
        this.#popup.addEventListener(
          "popupshown",
          () => {
            this.#popup.querySelector("toolbarbutton").focus();
          },
          { once: true }
        );
      }

      lazy.PanelMultiView.openPopup(this.#popup, anchor, {
        position: "bottomleft topleft",
        triggerEvent: event,
      }).catch(console.error);
    }
  }

  #openPreferences(event) {
    if (
      (event.type == "click" && event.button != 0) ||
      (event.type == "keypress" &&
        event.charCode != KeyEvent.DOM_VK_SPACE &&
        event.keyCode != KeyEvent.DOM_VK_RETURN)
    ) {
      return; // Left click, space or enter only
    }

    event.preventDefault();
    event.stopPropagation();

    this.#input.window.openPreferences("paneSearch");
    this.#popup.hidePopup();
  }

  /**
   * Exit the engine specific searchMode.
   *
   * @param {Event} event
   *        The event that triggered the searchMode exit.
   */
  exitSearchMode(event) {
    event.preventDefault();
    this.#input.searchMode = null;
    // Update the result by the default engine.
    this.#input.startQuery();
  }

  /**
   * Called when the value of the searchMode attribute on UrlbarInput is changed.
   */
  onSearchModeChanged() {
    if (lazy.UrlbarPrefs.get("scotchBonnet.enableOverride")) {
      this.#updateSearchIcon();
    }
  }

  handleEvent(event) {
    let action = event.currentTarget.dataset.action ?? event.type;

    switch (action) {
      case "openpopup": {
        this.openPanel(event);
        break;
      }
      case "exitsearchmode": {
        this.exitSearchMode(event);
        break;
      }
      case "openpreferences": {
        this.#openPreferences(event);
        break;
      }
    }
  }

  observe(_subject, topic, data) {
    switch (topic) {
      case "browser-search-engine-modified": {
        this.#engineListNeedsRebuild = true;
        if (data === "engine-default") {
          this.#updateSearchIcon();
        }
        break;
      }
    }
  }

  /**
   * Called when a urlbar pref changes.
   *
   * @param {string} pref
   *   The name of the pref relative to `browser.urlbar`.
   */
  onPrefChanged(pref) {
    switch (pref) {
      case "scotchBonnet.enableOverride": {
        if (lazy.UrlbarPrefs.get("scotchBonnet.enableOverride")) {
          this.#enableObservers();
        } else {
          this.#disableObservers();
        }
        break;
      }
      case "keyword.enabled": {
        if (lazy.UrlbarPrefs.get("scotchBonnet.enableOverride")) {
          this.#updateSearchIcon();
        }
        break;
      }
    }
  }

  async #updateSearchIcon() {
    try {
      await lazy.UrlbarSearchUtils.init();
    } catch {
      // We should still work if the SearchService is not working.
    }
    let { label, icon } = await this.#getDisplayedEngineDetails(
      this.#input.searchMode
    );

    const keywordEnabled = lazy.UrlbarPrefs.get("keyword.enabled");
    const inSearchMode = this.#input.searchMode;
    if (!keywordEnabled && !inSearchMode) {
      icon = lazy.UrlbarUtils.ICON.SEARCH_GLASS;
    }

    let iconUrl = icon ? `url(${icon})` : "";
    this.#input.document.getElementById(
      "searchmode-switcher-icon"
    ).style.listStyleImage = iconUrl;
    this.#input.document.l10n.setAttributes(
      this.#toolbarbutton,
      "urlbar-searchmode-button",
      { engine: label }
    );

    let labelEl = this.#input.document.getElementById(
      "searchmode-switcher-title"
    );

    if (!this.#input.searchMode) {
      labelEl.replaceChildren();
    } else if (this.#input.searchMode) {
      labelEl.textContent = label;
    }
  }

  async #getSearchModeLabel(source) {
    let mode = lazy.UrlbarUtils.LOCAL_SEARCH_MODES.find(
      m => m.source == source
    );
    let [str] = await lazy.SearchModeSwitcherL10n.formatMessages([
      { id: mode.uiLabel },
    ]);
    return str.attributes[0].value;
  }

  async #getDisplayedEngineDetails(searchMode = null) {
    if (!searchMode || searchMode.engineName) {
      let engine = searchMode
        ? lazy.UrlbarSearchUtils.getEngineByName(searchMode.engineName)
        : lazy.UrlbarSearchUtils.getDefaultEngine();
      return { label: engine.name, icon: await engine.getIconURL() };
    }

    let mode = lazy.UrlbarUtils.LOCAL_SEARCH_MODES.find(
      m => m.source == searchMode.source
    );
    return {
      label: await this.#getSearchModeLabel(searchMode.source),
      icon: mode.icon,
    };
  }

  async #rebuildSearchModeList() {
    let container = this.#popup.querySelector(".panel-subview-body");
    container.replaceChildren();
    let engines = await Services.search.getVisibleEngines();
    let frag = this.#input.document.createDocumentFragment();
    let remoteContainer = this.#input.document.createXULElement("vbox");
    remoteContainer.className = "remote-options";
    frag.appendChild(remoteContainer);

    let fireCommand = e => {
      if (e.keyCode == KeyEvent.DOM_VK_RETURN) {
        let event = e.target.ownerDocument.createEvent("xulcommandevent");
        event.initCommandEvent(
          "command",
          true,
          true,
          e.target.ownerGlobal,
          0,
          e.ctrlKey,
          e.altKey,
          e.shiftKey,
          e.metaKey,
          0,
          e,
          e.inputSource
        );
        e.target.dispatchEvent(event);
      }
    };

    for (let engine of engines) {
      if (engine.hideOneOffButton) {
        continue;
      }
      let menuitem =
        this.#input.window.document.createXULElement("toolbarbutton");
      menuitem.setAttribute("class", "subviewbutton subviewbutton-iconic");
      menuitem.setAttribute("label", engine.name);
      menuitem.setAttribute("tabindex", "0");
      menuitem.setAttribute("role", "menuitem");
      menuitem.engine = engine;
      menuitem.addEventListener("keypress", fireCommand);
      menuitem.addEventListener("command", e => {
        this.search({ engine, openEngineHomePage: e.shiftKey });
      });

      menuitem.setAttribute("image", await engine.getIconURL());
      remoteContainer.appendChild(menuitem);
    }
    // Add local options.
    let localContainer = this.#input.document.createXULElement("vbox");
    localContainer.className = "local-options";
    frag.appendChild(localContainer);
    for (let { source, pref, restrict } of lazy.UrlbarUtils
      .LOCAL_SEARCH_MODES) {
      if (!lazy.UrlbarPrefs.get(pref)) {
        continue;
      }
      let name = lazy.UrlbarUtils.getResultSourceName(source);
      let button = this.#input.document.createXULElement("toolbarbutton");
      button.id = `search-button-${name}`;
      button.setAttribute("class", "subviewbutton subviewbutton-iconic");
      button.setAttribute("tabindex", "0");
      button.setAttribute("role", "menuitem");
      let { icon } = await this.#getDisplayedEngineDetails({
        source,
        pref,
        restrict,
      });
      if (icon) {
        button.setAttribute("image", icon);
      }
      button.addEventListener("keypress", fireCommand);
      button.addEventListener("command", () => {
        this.search({ restrict });
      });

      this.#input.document.l10n.setAttributes(
        button,
        `urlbar-searchmode-${name}`,
        {
          restrict,
        }
      );

      button.restrict = restrict;
      localContainer.appendChild(button);
    }
    container.appendChild(frag);
  }

  search({ engine = null, restrict = null, openEngineHomePage = false } = {}) {
    let gBrowser = this.#input.window.gBrowser;
    let search = "";
    let opts = null;
    if (engine) {
      search =
        gBrowser.userTypedValue ?? gBrowser.selectedBrowser.searchTerms ?? "";
      opts = {
        searchEngine: engine,
        searchModeEntry: "searchbutton",
        openEngineHomePage,
      };
    } else if (restrict) {
      search = restrict + " " + (gBrowser.userTypedValue || "");
      opts = { searchModeEntry: "searchbutton" };
    }

    if (openEngineHomePage) {
      opts.focus = false;
      opts.startQuery = false;
    }

    this.#input.search(search, opts);

    if (openEngineHomePage) {
      this.#input.openEngineHomePage(search, {
        searchEngine: opts.searchEngine,
      });
    }

    this.#popup.hidePopup();
  }

  #enableObservers() {
    Services.obs.addObserver(this, "browser-search-engine-modified", true);

    this.#toolbarbutton.addEventListener("mousedown", this);
    this.#toolbarbutton.addEventListener("keypress", this);

    let closebutton = this.#input.document.querySelector(
      "#searchmode-switcher-close"
    );
    closebutton.addEventListener("mousedown", this);
    closebutton.addEventListener("keypress", this);

    let prefsbutton = this.#input.document.querySelector(
      "#searchmode-switcher-popup-search-settings-button"
    );
    prefsbutton.addEventListener("mousedown", this);
    prefsbutton.addEventListener("keypress", this);

    this.#input.window.addEventListener(
      "MozAfterPaint",
      () => this.#updateSearchIcon(),
      { once: true }
    );
  }

  #disableObservers() {
    Services.obs.removeObserver(this, "browser-search-engine-modified");

    this.#toolbarbutton.removeEventListener("mousedown", this);
    this.#toolbarbutton.removeEventListener("keypress", this);

    let closebutton = this.#input.document.querySelector(
      "#searchmode-switcher-close"
    );
    closebutton.removeEventListener("mousedown", this);
    closebutton.removeEventListener("keypress", this);

    let prefsbutton = this.#input.document.querySelector(
      "#searchmode-switcher-popup-search-settings-button"
    );
    prefsbutton.removeEventListener("mousedown", this);
    prefsbutton.removeEventListener("keypress", this);
  }
}
