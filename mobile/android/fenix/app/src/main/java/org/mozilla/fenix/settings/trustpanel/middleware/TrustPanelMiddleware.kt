/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelAction
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelState

/**
 * [Middleware] implementation for handling [TrustPanelAction] and managing the [TrustPanelState] for the menu
 * dialog.
 *
 * @param sessionUseCases [SessionUseCases] used to reload the page after toggling tracking protection.
 * @param trackingProtectionUseCases [TrackingProtectionUseCases] used to add/remove sites from the
 * tracking protection exceptions list.
 * @param scope [CoroutineScope] used to launch coroutines.
 */
class TrustPanelMiddleware(
    private val sessionUseCases: SessionUseCases,
    private val trackingProtectionUseCases: TrackingProtectionUseCases,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
) : Middleware<TrustPanelState, TrustPanelAction> {

    override fun invoke(
        context: MiddlewareContext<TrustPanelState, TrustPanelAction>,
        next: (TrustPanelAction) -> Unit,
        action: TrustPanelAction,
    ) {
        val currentState = context.state

        when (action) {
            TrustPanelAction.ToggleTrackingProtection -> toggleTrackingProtection(currentState)

            else -> Unit
        }

        next(action)
    }

    private fun toggleTrackingProtection(
        currentState: TrustPanelState,
    ) = scope.launch {
        currentState.sessionState?.let { session ->
            if (currentState.isTrackingProtectionEnabled) {
                trackingProtectionUseCases.addException(session.id)
            } else {
                trackingProtectionUseCases.removeException(session.id)
            }

            sessionUseCases.reload.invoke(session.id)
        }
    }
}
