/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.middleware

import androidx.navigation.NavHostController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelAction
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelState
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelStore
import org.mozilla.fenix.settings.trustpanel.ui.TRACKERS_PANEL_ROUTE

/**
 * [Middleware] implementation for handling navigating events based on [TrustPanelAction]s that are
 * dispatched to the [TrustPanelStore].
 *
 * @param navHostController [NavHostController] used for Compose navigation.
 * @param scope [CoroutineScope] used to launch coroutines.
 */
class TrustPanelNavigationMiddleware(
    private val navHostController: NavHostController,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.Main),
) : Middleware<TrustPanelState, TrustPanelAction> {

    @Suppress("CyclomaticComplexMethod", "LongMethod")
    override fun invoke(
        context: MiddlewareContext<TrustPanelState, TrustPanelAction>,
        next: (TrustPanelAction) -> Unit,
        action: TrustPanelAction,
    ) {
        next(action)

        scope.launch {
            when (action) {
                is TrustPanelAction.Navigate.Back -> navHostController.popBackStack()

                is TrustPanelAction.Navigate.TrackersPanel -> navHostController.navigate(route = TRACKERS_PANEL_ROUTE)

                else -> Unit
            }
        }
    }
}
