/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A common list of systems, surfaces, controls, etc. from which user
 * interactions with tab groups could originate. These "source" values
 * should be sent as extra data with tab group-related metrics events.
 */
const METRIC_SOURCE = Object.freeze({
  TAB_OVERFLOW_MENU: "tab_overflow",
  TAB_GROUP_MENU: "tab_group",
  UNKNOWN: "unknown",
});

export const TabGroupMetrics = {
  METRIC_SOURCE,
};
