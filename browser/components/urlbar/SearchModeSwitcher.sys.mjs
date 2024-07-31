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
  #input = null;

  constructor(input) {
    this.#input = input;

    this.QueryInterface = ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]);
    Services.obs.addObserver(this, "browser-search-engine-modified", true);

    let toolbarbutton = input.document.querySelector(
      "#urlbar-searchmode-switcher"
    );
    toolbarbutton.addEventListener("mousedown", this);
    toolbarbutton.addEventListener("keypress", this);

    let closebutton = input.document.querySelector(
      "#searchmode-switcher-close"
    );
    closebutton.addEventListener("mousedown", this);
    closebutton.addEventListener("keypress", this);

    input.window.addEventListener(
      "MozAfterPaint",
      () => this.#updateSearchIcon(),
      { once: true }
    );
  }

  /**
   * Open the SearchSwitcher popup.
   *
   * @param {Event} event
   *        The event that triggered the opening of the popup.
   */
  async openPanel(event) {
    let anchor = event.target;
    event.stopPropagation();

    if (this.#input.document.documentElement.hasAttribute("customizing")) {
      return;
    }

    if (this.#engineListNeedsRebuild) {
      await this.#rebuildSearchModeList(this.#input.window);
      this.#engineListNeedsRebuild = false;
    }
    if (anchor.getAttribute("open") != "true") {
      this.#input.view.hideTemporarily();

      this.#getPopup().addEventListener(
        "popuphidden",
        () => {
          anchor.removeAttribute("open");
          this.#input.view.restoreVisibility();
        },
        { once: true }
      );
      anchor.setAttribute("open", true);

      lazy.PanelMultiView.openPopup(this.#getPopup(), anchor, {
        position: "bottomleft topleft",
        triggerEvent: event,
      }).catch(console.error);
    }
  }

  /**
   * Exit the engine specific searchMode.
   *
   * @param {Event} event
   *        The event that triggered the searchMode exit.
   */
  exitSearchMode(event) {
    event.stopPropagation();
    event.preventDefault();
    this.#input.searchMode = null;
  }

  /**
   * Called when the value of the searchMode attribute on UrlbarInput is changed.
   */
  onSearchModeChanged() {
    this.#updateSearchIcon();
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
    }
  }

  observe(_subject, topic, _data) {
    switch (topic) {
      case "browser-search-engine-modified": {
        this.#engineListNeedsRebuild = true;
        break;
      }
    }
  }

  #popup = null;

  #getPopup() {
    if (!this.#popup) {
      this.#popup = this.#input.document.getElementById(
        "searchmode-switcher-popup"
      );
    }
    return this.#popup;
  }

  async #updateSearchIcon() {
    try {
      await lazy.UrlbarSearchUtils.init();
    } catch {
      // We should still work if the SearchService is not working.
    }
    let button = this.#input.document.getElementById(
      "searchmode-switcher-icon"
    );
    let iconURL = await this.#getIconForSearchMode(this.#input.searchMode);
    if (iconURL) {
      button.style.listStyleImage = `url(${iconURL})`;
    }
    let label = this.#input.document.getElementById(
      "searchmode-switcher-title"
    );
    if (!this.#input.searchMode) {
      label.replaceChildren();
    } else if (this.#input.searchMode?.engineName) {
      label.textContent = this.#input.searchMode.engineName;
    } else if (this.#input.searchMode?.source) {
      // Certainly the wrong way to do this, the same string is used
      // as a <label and as a <toolbarbutton label= and not sure
      // how to define an l10n ID to be used both ways.
      let mode = lazy.UrlbarUtils.LOCAL_SEARCH_MODES.find(
        m => m.source == this.#input.searchMode.source
      );
      let [str] = await lazy.SearchModeSwitcherL10n.formatMessages([
        {
          id: mode.uiLabel,
          args: { restrict: mode.restrict },
        },
      ]);
      label.textContent = str.attributes[0].value;
    }
  }

  async #getIconForSearchMode(searchMode = null) {
    if (!searchMode || searchMode.engineName) {
      let engine = searchMode
        ? lazy.UrlbarSearchUtils.getEngineByName(searchMode.engineName)
        : lazy.UrlbarSearchUtils.getDefaultEngine();
      return engine.getIconURL();
    }

    let mode = lazy.UrlbarUtils.LOCAL_SEARCH_MODES.find(
      m => m.source == searchMode.source
    );
    return mode.icon;
  }

  async #rebuildSearchModeList() {
    let container = this.#getPopup().querySelector(".panel-subview-body");
    container.replaceChildren();
    let engines = await Services.search.getVisibleEngines();
    let frag = this.#input.document.createDocumentFragment();
    let remoteContainer = this.#input.document.createXULElement("vbox");
    remoteContainer.className = "remote-options";
    frag.appendChild(remoteContainer);

    for (let engine of engines) {
      if (engine.hideOneOffButton) {
        continue;
      }
      let menuitem =
        this.#input.window.document.createXULElement("toolbarbutton");
      menuitem.setAttribute("class", "subviewbutton subviewbutton-iconic");
      menuitem.setAttribute("label", engine.name);
      menuitem.setAttribute("tabindex", "0");
      menuitem.engine = engine;
      menuitem.setAttribute(
        "oncommand",
        "gURLBar.searchModeSwitcher.search(event, { engine: this.engine })"
      );
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
      let iconUrl = await this.#getIconForSearchMode({
        source,
        pref,
        restrict,
      });
      if (iconUrl) {
        button.setAttribute("image", iconUrl);
      }
      button.setAttribute(
        "oncommand",
        "gURLBar.searchModeSwitcher.search(event, { restrict: this.restrict })"
      );
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

  search(event, { engine = null, restrict = null } = {}) {
    let gBrowser = this.#input.window.gBrowser;
    let search = "";
    let opts = null;
    if (engine) {
      search = gBrowser.userTypedValue || "";
      opts = { searchEngine: engine, searchModeEntry: "searchbutton" };
    } else if (restrict) {
      search = restrict + " " + (gBrowser.userTypedValue || "");
      opts = { searchModeEntry: "searchbutton" };
    }
    this.#input.search(search, opts);
    this.#getPopup().hidePopup();
  }
}
