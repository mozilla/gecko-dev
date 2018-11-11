/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Shared Places Import - change other consumers if you change this: */
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  LightweightThemeChild: "resource:///actors/LightweightThemeChild.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.jsm",
  PlacesTransactions: "resource://gre/modules/PlacesTransactions.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});
XPCOMUtils.defineLazyScriptGetter(this, "PlacesTreeView",
                                  "chrome://browser/content/places/treeView.js");
XPCOMUtils.defineLazyScriptGetter(this, ["PlacesInsertionPoint", "PlacesController",
                                         "PlacesControllerDragHelper"],
                                  "chrome://browser/content/places/controller.js");
/* End Shared Places Import */

var gHistoryTree;
var gSearchBox;
var gHistoryGrouping = "";
var gSearching = false;

function HistorySidebarInit() {
  let uidensity = window.top.document.documentElement.getAttribute("uidensity");
  if (uidensity) {
    document.documentElement.setAttribute("uidensity", uidensity);
  }

  /* Listen for sidebar theme changes */
  let themeListener = new LightweightThemeChild({
    content: window,
    chromeOuterWindowID: window.top.windowUtils.outerWindowID,
    docShell: window.docShell,
  });

  window.addEventListener("unload", () => {
    themeListener.cleanup();
  });

  gHistoryTree = document.getElementById("historyTree");
  gSearchBox = document.getElementById("search-box");

  gHistoryGrouping = document.getElementById("viewButton").
                              getAttribute("selectedsort");

  if (gHistoryGrouping == "site")
    document.getElementById("bysite").setAttribute("checked", "true");
  else if (gHistoryGrouping == "visited")
    document.getElementById("byvisited").setAttribute("checked", "true");
  else if (gHistoryGrouping == "lastvisited")
    document.getElementById("bylastvisited").setAttribute("checked", "true");
  else if (gHistoryGrouping == "dayandsite")
    document.getElementById("bydayandsite").setAttribute("checked", "true");
  else
    document.getElementById("byday").setAttribute("checked", "true");

  searchHistory("");
}

function GroupBy(groupingType) {
  gHistoryGrouping = groupingType;
  searchHistory(gSearchBox.value);
}

function searchHistory(aInput) {
  var query = PlacesUtils.history.getNewQuery();
  var options = PlacesUtils.history.getNewQueryOptions();

  const NHQO = Ci.nsINavHistoryQueryOptions;
  var sortingMode;
  var resultType;

  switch (gHistoryGrouping) {
    case "visited":
      resultType = NHQO.RESULTS_AS_URI;
      sortingMode = NHQO.SORT_BY_VISITCOUNT_DESCENDING;
      break;
    case "lastvisited":
      resultType = NHQO.RESULTS_AS_URI;
      sortingMode = NHQO.SORT_BY_DATE_DESCENDING;
      break;
    case "dayandsite":
      resultType = NHQO.RESULTS_AS_DATE_SITE_QUERY;
      break;
    case "site":
      resultType = NHQO.RESULTS_AS_SITE_QUERY;
      sortingMode = NHQO.SORT_BY_TITLE_ASCENDING;
      break;
    case "day":
    default:
      resultType = NHQO.RESULTS_AS_DATE_QUERY;
      break;
  }

  if (aInput) {
    query.searchTerms = aInput;
    if (gHistoryGrouping != "visited" && gHistoryGrouping != "lastvisited") {
      sortingMode = NHQO.SORT_BY_FRECENCY_DESCENDING;
      resultType = NHQO.RESULTS_AS_URI;
    }
  }

  options.sortingMode = sortingMode;
  options.resultType = resultType;
  options.includeHidden = !!aInput;

  if (gHistoryGrouping == "lastvisited")
    this.TelemetryStopwatch.start("HISTORY_LASTVISITED_TREE_QUERY_TIME_MS");

  // call load() on the tree manually
  // instead of setting the place attribute in historySidebar.xul
  // otherwise, we will end up calling load() twice
  gHistoryTree.load(query, options);

  if (gHistoryGrouping == "lastvisited")
    this.TelemetryStopwatch.finish("HISTORY_LASTVISITED_TREE_QUERY_TIME_MS");
}

window.addEventListener("SidebarFocused",
                        () => gSearchBox.focus());
