/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.store

import mozilla.components.lib.state.Action

/**
 * Actions to dispatch through the [TrustPanelStore] to modify the [TrustPanelState].
 */
sealed class TrustPanelAction : Action {

    /**
     * [TrustPanelAction] dispatched when tracking protection is toggled.
     */
    data object ToggleTrackingProtection : TrustPanelAction()

    /**
     * [TrustPanelAction] dispatched when a navigation event occurs for a specific destination.
     */
    sealed class Navigate : TrustPanelAction() {
        /**
         * [Navigate] action dispatched when a back navigation event occurs.
         */
        data object Back : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the trackers panel.
         */
        data object TrackersPanel : Navigate()
    }
}
