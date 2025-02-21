/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Shared Places Import - change other consumers if you change this: */
var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  PlacesTransactions: "resource://gre/modules/PlacesTransactions.sys.mjs",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

XPCOMUtils.defineLazyScriptGetter(
  this,
  "PlacesTreeView",
  "chrome://browser/content/places/treeView.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  ["PlacesInsertionPoint", "PlacesController", "PlacesControllerDragHelper"],
  "chrome://browser/content/places/controller.js"
);
/* End Shared Places Import */
var gCumulativeSearches = 0;

window.addEventListener("load", () => {
  let uidensity = window.top.document.documentElement.getAttribute("uidensity");
  if (uidensity) {
    document.documentElement.setAttribute("uidensity", uidensity);
  }

  let view = document.getElementById("bookmarks-view");
  view.place =
    "place:type=" + Ci.nsINavHistoryQueryOptions.RESULTS_AS_ROOTS_QUERY;
  view.addEventListener("keypress", event =>
    PlacesUIUtils.onSidebarTreeKeyPress(event)
  );
  view.addEventListener("click", event =>
    PlacesUIUtils.onSidebarTreeClick(event)
  );
  view.addEventListener("mousemove", event =>
    PlacesUIUtils.onSidebarTreeMouseMove(event)
  );
  view.addEventListener("mouseout", () =>
    PlacesUIUtils.setMouseoverURL("", window)
  );

  document
    .getElementById("search-box")
    .addEventListener("command", searchBookmarks);

  let bhTooltip = document.getElementById("bhTooltip");
  bhTooltip.addEventListener("popupshowing", event => {
    window.top.BookmarksEventHandler.fillInBHTooltip(bhTooltip, event);
  });
  bhTooltip.addEventListener("popuphiding", () =>
    bhTooltip.removeAttribute("position")
  );

  document
    .getElementById("sidebar-panel-close")
    .addEventListener("click", closeSidebarPanel);
});

function searchBookmarks(event) {
  let { value } = event.currentTarget;

  var tree = document.getElementById("bookmarks-view");
  if (!value) {
    // eslint-disable-next-line no-self-assign
    tree.place = tree.place;
  } else {
    Glean.sidebar.search.bookmarks.add(1);
    gCumulativeSearches++;
    tree.applyFilter(value, PlacesUtils.bookmarks.userContentRoots);
  }
}

function updateTelemetry(urlsOpened = []) {
  Glean.bookmarksSidebar.cumulativeSearches.accumulateSingleSample(
    gCumulativeSearches
  );
  clearCumulativeCounter();

  Glean.sidebar.link.bookmarks.add(urlsOpened.length);
}

function clearCumulativeCounter() {
  gCumulativeSearches = 0;
}

window.addEventListener("unload", () => {
  clearCumulativeCounter();
  PlacesUIUtils.setMouseoverURL("", window);
});

function closeSidebarPanel(e) {
  e.preventDefault();
  let view = e.target.getAttribute("view");
  window.browsingContext.embedderWindowGlobal.browsingContext.window.SidebarController.toggle(
    view
  );
}

window.addEventListener("SidebarFocused", () =>
  document.getElementById("search-box").focus()
);
