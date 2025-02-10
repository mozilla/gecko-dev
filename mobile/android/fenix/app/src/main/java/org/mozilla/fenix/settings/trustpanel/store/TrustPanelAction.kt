/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.store

import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.lib.state.Action

/**
 * Actions to dispatch through the [TrustPanelStore] to modify the [TrustPanelState].
 */
sealed class TrustPanelAction : Action {

    /**
     * [TrustPanelAction] dispatched when the current site's data is cleared.
     */
    data object ClearSiteData : TrustPanelAction()

    /**
     * [TrustPanelAction] dispatched when tracking protection is toggled.
     */
    data object ToggleTrackingProtection : TrustPanelAction()

    /**
     * [TrustPanelAction] dispatched when the clear site data dialog is requested.
     */
    data object RequestClearSiteDataDialog : TrustPanelAction()

    /**
     * [TrustPanelAction] dispatched when the site's base domain is updated for the clear site data dialog.
     *
     * @property baseDomain The base domain of the current site.
     */
    data class UpdateBaseDomain(val baseDomain: String) : TrustPanelAction()

    /**
     * [TrustPanelAction] dispatched when the number of blocked trackers is updated.
     *
     * @property newNumberOfTrackersBlocked Updated number of trackers that have been blocked
     */
    data class UpdateNumberOfTrackersBlocked(val newNumberOfTrackersBlocked: Int) : TrustPanelAction()

    /**
     * [TrustPanelAction] dispatched when the list of trackers is changed.
     *
     * @property trackerLogs The new list of blocked [TrackerLog]s.
     */
    data class UpdateTrackersBlocked(val trackerLogs: List<TrackerLog>) : TrustPanelAction()

    /**
     * [TrustPanelAction] dispatched when a navigation event occurs for a specific destination.
     */
    sealed class Navigate : TrustPanelAction() {
        /**
         * [Navigate] action dispatched when a back navigation event occurs.
         */
        data object Back : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the clear site data dialog.
         */
        data object ClearSiteDataDialog : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the trackers panel.
         */
        data object TrackersPanel : Navigate()
    }
}
