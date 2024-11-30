/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.store

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [TrustPanelState] and applying [TrustPanelAction]s.
 */
class TrustPanelStore(
    initialState: TrustPanelState = TrustPanelState(),
    middleware: List<Middleware<TrustPanelState, TrustPanelAction>> = emptyList(),
) : Store<TrustPanelState, TrustPanelAction>(
    initialState = initialState,
    reducer = ::reducer,
    middleware = middleware,
)

private fun reducer(state: TrustPanelState, action: TrustPanelAction): TrustPanelState {
    return when (action) {
        is TrustPanelAction.Navigate -> state
        is TrustPanelAction.ToggleTrackingProtection -> state.copy(
            isTrackingProtectionEnabled = !state.isTrackingProtectionEnabled,
        )
    }
}
