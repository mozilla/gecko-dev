/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PanelMultiView: "resource:///modules/PanelMultiView.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "SearchModeSwitcherL10n", () => {
  return new Localization(["preview/enUS-searchFeatures.ftl"]);
});

/**
 * Implements the SearchModeSwitcher in the urlbar.
 */
class _SearchModeSwitcher {
  #initialized = false;
  #engineListNeedsRebuild = true;

  /**
   * Initialise the SearchSwitcher, ensuring the correct engine favicon is shown.
   *
   * @param {DomeElement} win
   *        The window currently in use.
   */
  async init(win) {
    if (!this.#initialized) {
      await Services.search.init();
      Services.obs.addObserver(this, "browser-search-engine-modified");
      this.#initialized = true;
    }
    this.#updateSearchIcon(win);
  }

  /**
   * Open the SearchSwitcher popup.
   *
   * @param {DomeElement} anchor
   *        The element the popup is anchored to.
   * @param {Event} event
   *        The event that triggered the opening of the popup.
   */
  async openPanel(anchor, event) {
    let win = event.target.ownerGlobal;
    event.stopPropagation();

    if (win.document.documentElement.hasAttribute("customizing")) {
      return;
    }

    if (this.#engineListNeedsRebuild) {
      await this.#rebuildSearchModeList(win);
      this.#engineListNeedsRebuild = false;
    }
    if (anchor.getAttribute("open") != "true") {
      win.gURLBar.inputField.addEventListener("searchmodechanged", this);
      this.#getPopup(win).addEventListener(
        "popuphidden",
        () => {
          win.gURLBar.inputField.removeEventListener("searchmodechanged", this);
          anchor.removeAttribute("open");
        },
        { once: true }
      );
      anchor.setAttribute("open", true);

      lazy.PanelMultiView.openPopup(this.#getPopup(win), anchor, {
        position: "bottomleft topleft",
        triggerEvent: event,
      }).catch(console.error);
    }
  }

  /**
   * Exit the engine specific searchMode.
   *
   * @param {DomeElement} _anchor
   *        The element the popup is anchored to.
   * @param {Event} event
   *        The event that triggered the searchMode exit.
   */
  exitSearchMode(_anchor, event) {
    event.stopPropagation();
    event.preventDefault();
    event.target.ownerGlobal.gURLBar.searchMode = null;
    this.#updateSearchIcon(event.target.ownerGlobal);
  }

  handleEvent(event) {
    switch (event.type) {
      case "searchmodechanged": {
        this.#updateSearchIcon(event.target.ownerGlobal);
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

  #getPopup(win) {
    if (!this.#popup) {
      this.#popup = win.document.getElementById("searchmode-switcher-popup");
    }
    return this.#popup;
  }

  async #updateSearchIcon(win) {
    let button = win.document.getElementById("searchmode-switcher-icon");
    let iconURL = await this.#getIconForSearchMode(win.gURLBar.searchMode);
    if (iconURL) {
      button.style.listStyleImage = `url(${iconURL})`;
    }
    let label = win.document.getElementById("searchmode-switcher-title");
    if (!win.gURLBar.searchMode) {
      label.replaceChildren();
    } else if (win.gURLBar.searchMode?.engineName) {
      label.textContent = win.gURLBar.searchMode.engineName;
    } else if (win.gURLBar.searchMode?.source) {
      // Certainly the wrong way to do this, the same string is used
      // as a <label and as a <toolbarbutton label= and not sure
      // how to define an l10n ID to be used both ways.
      let mode = lazy.UrlbarUtils.LOCAL_SEARCH_MODES.find(
        m => m.source == win.gURLBar.searchMode.source
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
        ? Services.search.getEngineByName(searchMode.engineName)
        : Services.search.defaultEngine;
      return engine.getIconURL();
    }

    let mode = lazy.UrlbarUtils.LOCAL_SEARCH_MODES.find(
      m => m.source == searchMode.source
    );
    return mode.icon;
  }

  async #rebuildSearchModeList(win) {
    let container = this.#getPopup(win).querySelector(".panel-subview-body");
    container.replaceChildren();
    let engines = await Services.search.getVisibleEngines();
    let frag = win.document.createDocumentFragment();
    let remoteContainer = win.document.createXULElement("vbox");
    remoteContainer.className = "remote-options";
    frag.appendChild(remoteContainer);
    for (let engine of engines) {
      let menuitem = win.document.createXULElement("toolbarbutton");
      menuitem.setAttribute("class", "subviewbutton subviewbutton-iconic");
      menuitem.setAttribute("label", engine.name);
      menuitem.setAttribute("tabindex", "0");
      menuitem.engine = engine;
      menuitem.setAttribute(
        "oncommand",
        "SearchModeSwitcher.search(event, { engine: this.engine })"
      );
      menuitem.setAttribute("image", await engine.getIconURL());
      remoteContainer.appendChild(menuitem);
    }
    // Add local options.
    let localContainer = win.document.createXULElement("vbox");
    localContainer.className = "local-options";
    frag.appendChild(localContainer);
    for (let { source, pref, restrict } of lazy.UrlbarUtils
      .LOCAL_SEARCH_MODES) {
      if (!lazy.UrlbarPrefs.get(pref)) {
        continue;
      }
      let name = lazy.UrlbarUtils.getResultSourceName(source);
      let button = win.document.createXULElement("toolbarbutton");
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
        "SearchModeSwitcher.search(event, { restrict: this.restrict })"
      );
      win.document.l10n.setAttributes(button, `urlbar-searchmode-${name}`, {
        restrict,
      });

      button.restrict = restrict;
      localContainer.appendChild(button);
    }
    container.appendChild(frag);
  }

  search(event, { engine = null, restrict = null } = {}) {
    let win = event.target.ownerGlobal;
    if (engine) {
      win.gURLBar.search(win.gBrowser.userTypedValue || "", {
        searchEngine: engine,
        searchModeEntry: "searchbutton",
      });
    } else if (restrict) {
      win.gURLBar.search(restrict + " " + (win.gBrowser.userTypedValue || ""), {
        searchModeEntry: "searchbutton",
      });
    } else {
      win.gURLBar.search("");
    }

    this.#getPopup(win).hidePopup();
  }
}

export const SearchModeSwitcher = new _SearchModeSwitcher();
