#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

// The amount of time we wait while coalescing updates for hidden pages.
const SCHEDULE_UPDATE_TIMEOUT_MS = 1000;

/**
 * This singleton represents the whole 'New Tab Page' and takes care of
 * initializing all its components.
 */
let gPage = {
  /**
   * Initializes the page.
   */
  init: function Page_init() {
    // Add ourselves to the list of pages to receive notifications.
    gAllPages.register(this);

    // Listen for 'unload' to unregister this page.
    addEventListener("unload", this, false);

    // XXX bug 991111 - Not all click events are correctly triggered when
    // listening from xhtml nodes -- in particular middle clicks on sites, so
    // listen from the xul window and filter then delegate
    addEventListener("click", this, false);

    // Check if the new tab feature is enabled.
    let enabled = gAllPages.enabled;
    if (enabled)
      this._init();

    this._updateAttributes(enabled);

    // Initialize customize controls.
    gCustomize.init();

    // Initialize intro panel.
    gIntro.init();
  },

  /**
   * Listens for notifications specific to this page.
   */
  observe: function Page_observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed") {
      gCustomize.updateSelected();

      let enabled = gAllPages.enabled;
      this._updateAttributes(enabled);

      // Update thumbnails to the new enhanced setting
      if (aData == "browser.newtabpage.enhanced") {
        this.update();
      }

      // Initialize the whole page if we haven't done that, yet.
      if (enabled) {
        this._init();
      } else {
        gUndoDialog.hide();
      }
    } else if (aTopic == "page-thumbnail:create" && gGrid.ready) {
      for (let site of gGrid.sites) {
        if (site && site.url === aData) {
          site.refreshThumbnail();
        }
      }
    }
  },

  /**
   * Updates the page's grid right away for visible pages. If the page is
   * currently hidden, i.e. in a background tab or in the preloader, then we
   * batch multiple update requests and refresh the grid once after a short
   * delay. Accepts a single parameter the specifies the reason for requesting
   * a page update. The page may decide to delay or prevent a requested updated
   * based on the given reason.
   */
  update(reason = "") {
    // Update immediately if we're visible.
    if (!document.hidden) {
      // Ignore updates where reason=links-changed as those signal that the
      // provider's set of links changed. We don't want to update visible pages
      // in that case, it is ok to wait until the user opens the next tab.
      if (reason != "links-changed" && gGrid.ready) {
        gGrid.refresh();
      }

      return;
    }

    // Bail out if we scheduled before.
    if (this._scheduleUpdateTimeout) {
      return;
    }

    this._scheduleUpdateTimeout = setTimeout(() => {
      // Refresh if the grid is ready.
      if (gGrid.ready) {
        gGrid.refresh();
      }

      this._scheduleUpdateTimeout = null;
    }, SCHEDULE_UPDATE_TIMEOUT_MS);
  },

  /**
   * Internally initializes the page. This runs only when/if the feature
   * is/gets enabled.
   */
  _init: function Page_init() {
    if (this._initialized)
      return;

    this._initialized = true;

    // Initialize search.
    gSearch.init();

    if (document.hidden) {
      addEventListener("visibilitychange", this);
    } else {
      setTimeout(_ => this.onPageFirstVisible());
    }

    // Initialize and render the grid.
    gGrid.init();

    // Initialize the drop target shim.
    gDropTargetShim.init();

#ifdef XP_MACOSX
    // Workaround to prevent a delay on MacOSX due to a slow drop animation.
    document.addEventListener("dragover", this, false);
    document.addEventListener("drop", this, false);
#endif
  },

  /**
   * Updates the 'page-disabled' attributes of the respective DOM nodes.
   * @param aValue Whether the New Tab Page is enabled or not.
   */
  _updateAttributes: function Page_updateAttributes(aValue) {
    // Set the nodes' states.
    let nodeSelector = "#newtab-scrollbox, #newtab-grid, #newtab-search-container";
    for (let node of document.querySelectorAll(nodeSelector)) {
      if (aValue)
        node.removeAttribute("page-disabled");
      else
        node.setAttribute("page-disabled", "true");
    }

    // Enables/disables the control and link elements.
    let inputSelector = ".newtab-control, .newtab-link";
    for (let input of document.querySelectorAll(inputSelector)) {
      if (aValue) 
        input.removeAttribute("tabindex");
      else
        input.setAttribute("tabindex", "-1");
    }
  },

  /**
   * Handles all page events.
   */
  handleEvent: function Page_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "load":
        this.onPageVisibleAndLoaded();
        break;
      case "unload":
        gAllPages.unregister(this);
        break;
      case "click":
        let {button, target} = aEvent;
        // Go up ancestors until we find a Site or not
        while (target) {
          if (target.hasOwnProperty("_newtabSite")) {
            target._newtabSite.onClick(aEvent);
            break;
          }
          target = target.parentNode;
        }
        break;
      case "dragover":
        if (gDrag.isValid(aEvent) && gDrag.draggedSite)
          aEvent.preventDefault();
        break;
      case "drop":
        if (gDrag.isValid(aEvent) && gDrag.draggedSite) {
          aEvent.preventDefault();
          aEvent.stopPropagation();
        }
        break;
      case "visibilitychange":
        // Cancel any delayed updates for hidden pages now that we're visible.
        if (this._scheduleUpdateTimeout) {
          clearTimeout(this._scheduleUpdateTimeout);
          this._scheduleUpdateTimeout = null;

          // An update was pending so force an update now.
          this.update();
        }

        setTimeout(() => this.onPageFirstVisible());
        removeEventListener("visibilitychange", this);
        break;
    }
  },

  onPageFirstVisible: function () {
    // Record another page impression.
    Services.telemetry.getHistogramById("NEWTAB_PAGE_SHOWN").add(true);

    for (let site of gGrid.sites) {
      if (site) {
        // The site may need to modify and/or re-render itself if
        // something changed after newtab was created by preloader.
        // For example, the suggested tile endTime may have passed.
        site.onFirstVisible();
      }
    }

    if (document.readyState == "complete") {
      this.onPageVisibleAndLoaded();
    } else {
      addEventListener("load", this);
    }
  },

  onPageVisibleAndLoaded() {
    // Send the index of the last visible tile.
    this.reportLastVisibleTileIndex();

    // Show the panel now that anchors are sized
    gIntro.showIfNecessary();
  },

  reportLastVisibleTileIndex() {
    let cwu = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);

    let rect = cwu.getBoundsWithoutFlushing(gGrid.node);
    let nodes = cwu.nodesFromRect(rect.left, rect.top, 0, rect.width,
                                  rect.height, 0, true, false);

    let i = -1;
    let lastIndex = -1;
    let sites = gGrid.sites;

    for (let node of nodes) {
      if (node.classList && node.classList.contains("newtab-cell")) {
        if (sites[++i]) {
          lastIndex = i;
        }
      }
    }

    DirectoryLinksProvider.reportSitesAction(sites, "view", lastIndex);
  }
};
