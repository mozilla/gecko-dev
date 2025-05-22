/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A common list of systems, surfaces, controls, etc. from which user
 * interactions with tabs could originate. These "source" values
 * should be sent as extra data with tab-related metrics events.
 */
const METRIC_SOURCE = Object.freeze({
  // Tab overflow menu/"list all tabs" menu
  TAB_OVERFLOW_MENU: "tab_overflow",
  // Tab group context menu (right-clicking on a tab group label)
  TAB_GROUP_MENU: "tab_group",
  CANCEL_TAB_GROUP_CREATION: "cancel_create",
  // Tab context menu (right-clicking on a tab)
  TAB_MENU: "tab_menu",
  TAB_STRIP: "tab_strip",
  DRAG_AND_DROP: "drag",
  // "Search & Suggest," i.e. URL bar suggested actions while typing
  SUGGEST: "suggest",
  // History > Recently Closed Tabs menu, undo recently closed tab, etc.
  RECENT_TABS: "recent",
  UNKNOWN: "unknown",
});

const METRIC_TABS_LAYOUT = Object.freeze({
  HORIZONTAL: "horizontal",
  VERTICAL: "vertical",
});

const METRIC_REOPEN_TYPE = Object.freeze({
  SAVED: "saved",
  DELETED: "deleted",
});

/**
 * @typedef {object} TabMetricsContext
 * @property {boolean} [isUserTriggered=false]
 *   Should be true if there was an explicit action/request from the user
 *   (as opposed to some action being taken internally or for technical
 *   bookkeeping reasons alone). This causes telemetry events to fire.
 * @property {string} [telemetrySource="unknown"]
 *   The system, surface, or control the user used to take this action.
 *   @see TabMetrics.METRIC_SOURCE for possible values.
 *   Defaults to "unknown".
 */

/**
 * Creates a `TabMetricsContext` object for a user event originating from
 * the specified source.
 *
 * @param {string} telemetrySource
 *   @see TabMetrics.METRIC_SOURCE
 * @returns {TabMetricsContext}
 */
function userTriggeredContext(telemetrySource) {
  telemetrySource = telemetrySource || METRIC_SOURCE.UNKNOWN;
  return {
    isUserTriggered: true,
    telemetrySource,
  };
}

export const TabMetrics = {
  METRIC_SOURCE,
  METRIC_TABS_LAYOUT,
  METRIC_REOPEN_TYPE,
  userTriggeredContext,
};
