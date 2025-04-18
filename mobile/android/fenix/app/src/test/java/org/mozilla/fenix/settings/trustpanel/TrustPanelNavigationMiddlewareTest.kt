/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel

import androidx.navigation.NavController
import androidx.navigation.NavHostController
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.settings.trustpanel.middleware.TrustPanelNavigationMiddleware
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelAction
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelState
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelStore
import org.mozilla.fenix.settings.trustpanel.ui.CLEAR_SITE_DATA_DIALOG_ROUTE
import org.mozilla.fenix.settings.trustpanel.ui.CONNECTION_SECURITY_PANEL_ROUTE
import org.mozilla.fenix.settings.trustpanel.ui.TRACKERS_PANEL_ROUTE
import org.mozilla.fenix.settings.trustpanel.ui.TRACKER_CATEGORY_DETAILS_PANEL_ROUTE

class TrustPanelNavigationMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private val navController: NavController = mockk(relaxed = true)
    private val navHostController: NavHostController = mockk(relaxed = true)

    @Test
    fun `WHEN navigate back action is dispatched THEN pop back stack`() = runTest {
        val store = createStore()
        store.dispatch(TrustPanelAction.Navigate.Back).join()

        verify { navHostController.popBackStack() }
    }

    @Test
    fun `WHEN navigate to trackers panel action is dispatched THEN navigate to trackers panel route`() = runTest {
        val store = createStore()
        store.dispatch(TrustPanelAction.Navigate.TrackersPanel).join()

        verify {
            navHostController.navigate(route = TRACKERS_PANEL_ROUTE)
        }
    }

    @Test
    fun `WHEN navigate to connection security panel action is dispatched THEN navigate to connection security panel route`() = runTest {
        val store = createStore()
        store.dispatch(TrustPanelAction.Navigate.ConnectionSecurityPanel).join()

        verify {
            navHostController.navigate(route = CONNECTION_SECURITY_PANEL_ROUTE)
        }
    }

    @Test
    fun `WHEN navigate to clear site data dialog action is dispatched THEN navigate to clear site data dialog route`() = runTest {
        val store = createStore()
        store.dispatch(TrustPanelAction.Navigate.ClearSiteDataDialog).join()

        verify {
            navHostController.navigate(route = CLEAR_SITE_DATA_DIALOG_ROUTE)
        }
    }

    @Test
    fun `WHEN navigate to tracker category details panel action is dispatched THEN navigate to tracker category details panel route`() = runTest {
        val store = createStore()
        store.dispatch(TrustPanelAction.Navigate.TrackerCategoryDetailsPanel).join()

        verify {
            navHostController.navigate(route = TRACKER_CATEGORY_DETAILS_PANEL_ROUTE)
        }
    }

    @Test
    fun `WHEN navigate to privacy security settings action is dispatched THEN navigate to privacy and security settings`() = runTest {
        val privacySecurityPrefKey = "pref_key_privacy_security_category"
        val store = createStore(privacySecurityPrefKey = privacySecurityPrefKey)
        store.dispatch(TrustPanelAction.Navigate.PrivacySecuritySettings).join()

        verify {
            navController.nav(
                R.id.trustPanelFragment,
                TrustPanelFragmentDirections.actionGlobalSettingsFragment(
                    preferenceToScrollTo = privacySecurityPrefKey,
                ),
            )
        }
    }

    private fun createStore(
        trustPanelState: TrustPanelState = TrustPanelState(),
        privacySecurityPrefKey: String = "",
    ) = TrustPanelStore(
        initialState = trustPanelState,
        middleware = listOf(
            TrustPanelNavigationMiddleware(
                navController = navController,
                navHostController = navHostController,
                privacySecurityPrefKey = privacySecurityPrefKey,
                scope = scope,
            ),
        ),
    )
}
