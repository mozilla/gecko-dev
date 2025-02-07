/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createSelector,
} = require("resource://devtools/client/shared/vendor/reselect.js");
const {
  REQUESTS_WATERFALL,
} = require("resource://devtools/client/netmonitor/src/constants.js");

const EPSILON = 0.001;

const getWaterfallScale = createSelector(
  state => state.requests.firstStartedMs,
  state => state.requests.lastEndedMs,
  state => state.timingMarkers.firstDocumentDOMContentLoadedTimestamp,
  state => state.timingMarkers.firstDocumentLoadTimestamp,
  state => state.ui.waterfallWidth,
  (
    firstStartedMs,
    lastEndedMs,
    firstDocumentDOMContentLoadedTimestamp,
    firstDocumentLoadTimestamp,
    waterfallWidth
  ) => {
    if (firstStartedMs === +Infinity || waterfallWidth === null) {
      return null;
    }

    const lastEventMs = Math.max(
      lastEndedMs,
      firstDocumentDOMContentLoadedTimestamp,
      firstDocumentLoadTimestamp
    );
    const longestWidth = lastEventMs - firstStartedMs;

    // Reduce 20px for the last request's requests-list-timings-total
    return Math.min(
      Math.max(
        (waterfallWidth - REQUESTS_WATERFALL.LABEL_WIDTH - 20) / longestWidth,
        EPSILON
      ),
      1
    );
  }
);

function getVisibleColumns(columns) {
  return Object.entries(columns).filter(([_, shown]) => shown);
}

const getColumns = createSelector(
  state => state.ui.columns,
  state => state.ui.networkDetailsOpen || state.search.panelOpen,
  (_, hasOverride) => hasOverride,
  (columns, isSidePanelOpen, hasOverride) => {
    columns = { ...columns };
    const isWaterfallOnly =
      getVisibleColumns(columns).length === 1 && columns.waterfall;
    if (isSidePanelOpen && !isWaterfallOnly) {
      // Remove the Waterfall column if it is not the only column and a side
      // panel is open.
      delete columns.waterfall;
    }

    // Automatically add the override column if any override is currently
    // configured in the toolbox.
    columns.override = hasOverride;
    return columns;
  }
);

module.exports = {
  getColumns,
  getVisibleColumns,
  getWaterfallScale,
};
