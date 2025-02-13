/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.store

import mozilla.components.browser.state.state.SessionState
import mozilla.components.lib.state.State
import org.mozilla.fenix.trackingprotection.TrackerBuckets
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory

/**
 * Value type that represents the state of the unified trust panel.
 *
 * @property baseDomain The base domain of the current site used to display the clear site data dialog.
 * @property isTrackingProtectionEnabled Flag indicating whether enhanced tracking protection is enabled.
 * @property numberOfTrackersBlocked The numbers of trackers blocked by enhanced tracking protection.
 * @property bucketedTrackers Mapping of trackers sorted into different tracking protection categories.
 * @property detailedTrackerCategory The [TrackingProtectionCategory] which will be shown in the tracker
 * category details panel.
 * @property sessionState The [SessionState] of the current tab.
 */
data class TrustPanelState(
    val baseDomain: String? = null,
    val isTrackingProtectionEnabled: Boolean = true,
    val numberOfTrackersBlocked: Int = 0,
    val bucketedTrackers: TrackerBuckets = TrackerBuckets(),
    val detailedTrackerCategory: TrackingProtectionCategory? = null,
    val sessionState: SessionState? = null,
) : State
