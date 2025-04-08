/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import androidx.navigation.NavController
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.ext.nav

@RunWith(AndroidJUnit4::class)
class BrowserToolbarMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val genericInteractionEvent = object : BrowserToolbarEvent {}

    @Test
    fun `WHEN initializing the toolbar THEN add a menu button`() = runTestOnMain {
        val expectedMenuButton = ActionButton(
            icon = R.drawable.mozac_ic_ellipsis_vertical_24,
            contentDescription = R.string.content_description_menu,
            tint = R.attr.actionPrimary,
            onClick = genericInteractionEvent,
        )
        val middleware = BrowserToolbarMiddleware()
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

        val toolbarBrowserActions = toolbarStore.state.displayState.browserActions
        assertEquals(1, toolbarBrowserActions.size)
        assertSameButtonConfiguration(expectedMenuButton, toolbarBrowserActions[0] as ActionButton)
    }

    @Test
    fun `WHEN clicking the menu button THEN open the menu`() = runTestOnMain {
        val navController: NavController = mockk(relaxed = true)
        val middleware = BrowserToolbarMiddleware().apply {
            updateLifecycleDependencies(navController)
        }
        val toolbarStore = BrowserToolbarStore(
            middleware = listOf(middleware),
        )

        val menuAction = toolbarStore.state.displayState.browserActions[0] as ActionButton
        toolbarStore.dispatch(menuAction.onClick as BrowserToolbarEvent)

        verify {
            navController.nav(
                R.id.browserFragment,
                BrowserFragmentDirections.actionGlobalMenuDialogFragment(
                    accesspoint = MenuAccessPoint.Browser,
                ),
            )
        }
    }

    private fun assertSameButtonConfiguration(expected: ActionButton, actual: ActionButton) {
        assertEquals(expected.icon, actual.icon)
        assertEquals(expected.contentDescription, actual.contentDescription)
        assertEquals(expected.tint, actual.tint)

        // The actual click action is an implementation detail that we'll avoid to check.
        // Checking separately the outcome of these actions is a more useful test.
        // assertEquals(expected.onClick, actual.onClick)
        // assertEquals(expected.onLongClick, actual.onLongClick)
    }
}
