/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { ViewPage } from "./viewpage.mjs";

class RecentBrowsingInView extends ViewPage {
  constructor() {
    super();
    this.pageType = "recentbrowsing";
  }

  static queries = {
    searchTextbox: "moz-input-search",
  };

  static properties = {
    ...ViewPage.properties,
  };

  viewVisibleCallback() {
    for (let child of this.children) {
      let childView = child.firstElementChild;
      childView.paused = false;
      childView.viewVisibleCallback();
    }
  }

  viewHiddenCallback() {
    for (let child of this.children) {
      let childView = child.firstElementChild;
      childView.paused = true;
      childView.viewHiddenCallback();
    }
  }

  onSearchQuery() {
    // Individual components also listen to this event, so no need to do
    // anything else.
    Glean.firefoxviewNext.searchInitiatedSearch.record({
      page: "recentbrowsing",
    });
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/firefoxview.css"
      />
      <div class="sticky-container bottom-fade">
        <h2 class="page-header" data-l10n-id="firefoxview-overview-header"></h2>
        <div class="search-container">
          <moz-input-search
            data-l10n-id="firefoxview-search-text-box-recentbrowsing"
            data-l10n-attrs="placeholder"
            @MozInputSearch:search=${this.onSearchQuery}
          ></moz-input-search>
        </div>
      </div>
      <div class="cards-container">
        <slot></slot>
      </div>
    `;
  }
}
customElements.define("view-recentbrowsing", RecentBrowsingInView);
