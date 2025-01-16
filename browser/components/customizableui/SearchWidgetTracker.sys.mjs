/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { CustomizableUI } from "resource:///modules/CustomizableUI.sys.mjs";

const WIDGET_ID = "search-container";

/**
 * Updates persisted widths when the search bar is removed from any
 * customizable area and put into the palette. Also automatically removes the
 * search bar if it has not been used in a long time.
 */
export const SearchWidgetTracker = {
  /**
   * Main entrypoint to initializing the SearchWidgetTracker.
   */
  init() {
    CustomizableUI.addListener(this);
    this._updateSearchBarVisibilityBasedOnUsage();
  },

  /**
   * The callback for when a widget is moved via CustomizableUI. We use this
   * to detect movement of the search bar.
   *
   * @param {Element} node
   *   The DOM node that was acted upon.
   * @param {Element|null} _nextNode
   *   The DOM node (if any) that the widget was inserted before.
   * @param {Element} _container
   *   The *actual* DOM container for the widget (could be an overflow panel in
   *   case of an overflowable toolbar).
   * @param {boolean} wasRemoval
   *   True iff the action that happened was the removal of the DOM node.
   */
  onWidgetAfterDOMChange(node, _nextNode, _container, wasRemoval) {
    if (wasRemoval && node.id == WIDGET_ID) {
      this.removePersistedWidths();
    }
  },

  /**
   * If the search bar is in the navigation toolbar, this method will check
   * the lastUsed preference to see when the last time the search bar was
   * actually used. If the number of days since it was last used exceeds a
   * certain threshold, the widget is moved back into the customization
   * palette.
   */
  _updateSearchBarVisibilityBasedOnUsage() {
    if (!this._widgetIsInNavBar) {
      return;
    }
    let searchBarLastUsed = Services.prefs.getStringPref(
      "browser.search.widget.lastUsed",
      ""
    );
    if (searchBarLastUsed) {
      const removeAfterDaysUnused = Services.prefs.getIntPref(
        "browser.search.widget.removeAfterDaysUnused"
      );
      let saerchBarUnusedThreshold =
        removeAfterDaysUnused * 24 * 60 * 60 * 1000;
      if (new Date() - new Date(searchBarLastUsed) > saerchBarUnusedThreshold) {
        CustomizableUI.removeWidgetFromArea(WIDGET_ID);
      }
    }
  },

  /**
   * Removes any widget customization on the search bar (which can be created
   * with the resizer that appears if the search bar is placed immediately after
   * the URL bar). Goes through each open browser window and removes the width
   * property / style on each existant search bar.
   */
  removePersistedWidths() {
    Services.xulStore.removeValue(
      AppConstants.BROWSER_CHROME_URL,
      WIDGET_ID,
      "width"
    );
    for (let win of CustomizableUI.windows) {
      let searchbar =
        win.document.getElementById(WIDGET_ID) ||
        win.gNavToolbox.palette.querySelector("#" + WIDGET_ID);
      searchbar.removeAttribute("width");
      searchbar.style.removeProperty("width");
    }
  },

  /**
   * True if the search bar is currently in the navigation toolbar area.
   *
   * @type {boolean}
   */
  get _widgetIsInNavBar() {
    let placement = CustomizableUI.getPlacementOfWidget(WIDGET_ID);
    return placement?.area == CustomizableUI.AREA_NAVBAR;
  },
};
