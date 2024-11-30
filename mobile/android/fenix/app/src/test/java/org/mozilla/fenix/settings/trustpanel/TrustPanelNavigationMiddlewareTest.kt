/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel

import androidx.navigation.NavHostController
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.settings.trustpanel.middleware.TrustPanelNavigationMiddleware
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelAction
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelState
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelStore
import org.mozilla.fenix.settings.trustpanel.ui.TRACKERS_PANEL_ROUTE

class TrustPanelNavigationMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private val navHostController: NavHostController = mockk(relaxed = true)

    @Test
    fun `WHEN navigate back action is dispatched THEN pop back stack`() = runTest {
        val store = createStore()
        store.dispatch(TrustPanelAction.Navigate.Back).join()

        verify { navHostController.popBackStack() }
    }

    @Test
    fun `WHEN navigate to save action is dispatched THEN navigate to save submenu route`() = runTest {
        val store = createStore()
        store.dispatch(TrustPanelAction.Navigate.TrackersPanel).join()

        verify {
            navHostController.navigate(route = TRACKERS_PANEL_ROUTE)
        }
    }

    private fun createStore(
        trustPanelState: TrustPanelState = TrustPanelState(),
    ) = TrustPanelStore(
        initialState = trustPanelState,
        middleware = listOf(
            TrustPanelNavigationMiddleware(
                navHostController = navHostController,
                scope = scope,
            ),
        ),
    )
}
