/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.middleware

import androidx.navigation.NavController
import androidx.navigation.NavHostController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.settings.trustpanel.TrustPanelFragmentDirections
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelAction
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelState
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelStore
import org.mozilla.fenix.settings.trustpanel.ui.CLEAR_SITE_DATA_DIALOG_ROUTE
import org.mozilla.fenix.settings.trustpanel.ui.CONNECTION_SECURITY_PANEL_ROUTE
import org.mozilla.fenix.settings.trustpanel.ui.TRACKERS_PANEL_ROUTE
import org.mozilla.fenix.settings.trustpanel.ui.TRACKER_CATEGORY_DETAILS_PANEL_ROUTE

/**
 * [Middleware] implementation for handling navigating events based on [TrustPanelAction]s that are
 * dispatched to the [TrustPanelStore].
 *
 * @param navController [NavController] used for navigation.
 * @param navHostController [NavHostController] used for Compose navigation.
 * @param privacySecurityPrefKey Preference key used to scroll to the Privacy and security category within settings.
 * @param scope [CoroutineScope] used to launch coroutines.
 */
class TrustPanelNavigationMiddleware(
    private val navController: NavController,
    private val navHostController: NavHostController,
    private val privacySecurityPrefKey: String,
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

                is TrustPanelAction.Navigate.ClearSiteDataDialog -> navHostController.navigate(
                    route = CLEAR_SITE_DATA_DIALOG_ROUTE,
                )

                is TrustPanelAction.Navigate.TrackerCategoryDetailsPanel -> navHostController.navigate(
                    route = TRACKER_CATEGORY_DETAILS_PANEL_ROUTE,
                )

                is TrustPanelAction.Navigate.TrackersPanel -> navHostController.navigate(
                    route = TRACKERS_PANEL_ROUTE,
                )

                is TrustPanelAction.Navigate.ConnectionSecurityPanel -> navHostController.navigate(
                    route = CONNECTION_SECURITY_PANEL_ROUTE,
                )

                is TrustPanelAction.Navigate.PrivacySecuritySettings -> navController.nav(
                    R.id.trustPanelFragment,
                    TrustPanelFragmentDirections.actionGlobalSettingsFragment(
                        preferenceToScrollTo = privacySecurityPrefKey,
                    ),
                )

                else -> Unit
            }
        }
    }
}
