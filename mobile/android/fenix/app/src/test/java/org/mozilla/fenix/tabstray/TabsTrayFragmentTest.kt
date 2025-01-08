/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.content.res.Configuration
import android.view.LayoutInflater
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkStatic
import io.mockk.verify
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.databinding.FragmentTabTrayDialogBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixGleanTestRule
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.helpers.MockkRetryTestRule
import org.mozilla.fenix.home.HomeScreenViewModel

@RunWith(FenixRobolectricTestRunner::class)
class TabsTrayFragmentTest {
    private lateinit var context: Context
    private lateinit var fragment: TabsTrayFragment
    private lateinit var tabsTrayDialogBinding: FragmentTabTrayDialogBinding

    @get:Rule
    val mockkRule = MockkRetryTestRule()

    @get:Rule
    val gleanTestRule = FenixGleanTestRule(testContext)

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        val inflater = LayoutInflater.from(testContext)
        tabsTrayDialogBinding = FragmentTabTrayDialogBinding.inflate(inflater)

        fragment = spyk(TabsTrayFragment())
        fragment._tabsTrayDialogBinding = tabsTrayDialogBinding
        every { fragment.context } returns context
        every { fragment.context } returns context
        every { fragment.viewLifecycleOwner } returns mockk(relaxed = true)
    }

    @Test
    fun `WHEN dismissTabsTrayAndNavigateHome is called with a sessionId THEN it navigates to home to delete that sessions and dismisses the tray`() {
        every { fragment.navigateToHomeAndDeleteSession(any()) } just Runs
        every { fragment.dismissTabsTray() } just Runs

        fragment.dismissTabsTrayAndNavigateHome("test")

        verify { fragment.navigateToHomeAndDeleteSession("test") }
        verify { fragment.dismissTabsTray() }
    }

    @Test
    fun `WHEN navigateToHomeAndDeleteSession is called with a sessionId THEN it navigates to home and transmits there the sessionId`() {
        try {
            mockkStatic("androidx.fragment.app.FragmentViewModelLazyKt")
            mockkStatic("androidx.navigation.fragment.FragmentKt")
            mockkStatic("org.mozilla.fenix.ext.NavControllerKt")
            val viewModel: HomeScreenViewModel = mockk(relaxed = true)
            every { fragment.homeViewModel } returns viewModel
            val navController: NavController = mockk(relaxed = true)
            every { fragment.findNavController() } returns navController

            fragment.navigateToHomeAndDeleteSession("test")

            verify { viewModel.sessionToDelete = "test" }
            verify { navController.navigate(NavGraphDirections.actionGlobalHome()) }
        } finally {
            unmockkStatic("org.mozilla.fenix.ext.NavControllerKt")
            unmockkStatic("androidx.navigation.fragment.FragmentKt")
            unmockkStatic("androidx.fragment.app.FragmentViewModelLazyKt")
        }
    }

    @Test
    fun `WHEN dismissTabsTray is called THEN it dismisses the tray`() {
        every { fragment.dismissAllowingStateLoss() } just Runs
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().components } returns mockk(relaxed = true)
            fragment.dismissTabsTray()

            verify { fragment.dismissAllowingStateLoss() }
        }
    }

    @Test
    fun `WHEN onConfigurationChanged is called THEN it delegates the tray behavior manager to update the tray`() {
        val trayBehaviorManager: TabSheetBehaviorManager = mockk(relaxed = true)
        fragment.trayBehaviorManager = trayBehaviorManager
        val newConfiguration = Configuration()
        every { context.settings().gridTabView } returns false

        fragment.onConfigurationChanged(newConfiguration)

        verify { trayBehaviorManager.updateDependingOnOrientation(newConfiguration.orientation) }
    }

    @Test
    fun `GIVEN a list of tabs WHEN a tab is present with an ID THEN the index is returned`() {
        val tab1 = TabSessionState(
            id = "tab1",
            content = ContentState(
                url = "https://mozilla.org",
                private = false,
                isProductUrl = false,
            ),
        )
        val tab2 = TabSessionState(
            id = "tab2",
            content = ContentState(
                url = "https://mozilla.org",
                private = false,
                isProductUrl = false,
            ),
        )
        val tab3 = TabSessionState(
            id = "tab3",
            content = ContentState(
                url = "https://mozilla.org",
                private = false,
                isProductUrl = false,
            ),
        )
        val tabsList = listOf(
            tab1,
            tab2,
            tab3,
        )
        val position = fragment.getTabPositionFromId(tabsList, "tab2")
        assertEquals(1, position)
    }
}
