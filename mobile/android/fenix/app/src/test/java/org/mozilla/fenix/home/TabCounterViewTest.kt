/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import androidx.navigation.NavController
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.tabcounter.TabCounter
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.StartOnHome
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.browsingmode.DefaultBrowsingModeManager
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixGleanTestRule
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class TabCounterViewTest {

    @get:Rule
    val gleanTestRule = FenixGleanTestRule(testContext)

    private lateinit var navController: NavController
    private lateinit var browsingModeManager: BrowsingModeManager
    private lateinit var settings: Settings
    private lateinit var modeDidChange: (BrowsingMode) -> Unit
    private lateinit var tabCounterView: TabCounterView
    private lateinit var tabCounter: TabCounter

    @Before
    fun setup() {
        navController = mockk(relaxed = true)
        settings = mockk(relaxed = true)
        modeDidChange = mockk(relaxed = true)

        tabCounter = spyk(TabCounter(testContext))

        browsingModeManager = DefaultBrowsingModeManager(
            initialMode = BrowsingMode.Normal,
            settings = settings,
            modeDidChange = modeDidChange,
        )
    }

    @Test
    fun `WHEN tab counter is clicked THEN navigate to tabs tray and record metrics`() {
        setupToolbarView()
        every { navController.currentDestination?.id } returns R.id.homeFragment

        assertNull(StartOnHome.openTabsTray.testGetValue())

        tabCounter.performClick()

        assertNotNull(StartOnHome.openTabsTray.testGetValue())

        verify {
            navController.nav(
                R.id.homeFragment,
                NavGraphDirections.actionGlobalTabsTrayFragment(),
            )
        }
    }

    @Test
    fun `WHEN New tab menu item is tapped THEN set browsing mode to normal`() {
        setupToolbarView()
        tabCounterView.onItemTapped(TabCounterMenu.Item.NewTab)

        assertEquals(BrowsingMode.Normal, browsingModeManager.mode)
    }

    @Test
    fun `WHEN New private tab menu item is tapped THEN set browsing mode to private`() {
        setupToolbarView()
        tabCounterView.onItemTapped(TabCounterMenu.Item.NewPrivateTab)

        assertEquals(BrowsingMode.Private, browsingModeManager.mode)
    }

    @Test
    fun `WHEN tab counter is updated THEN set the tab counter to the correct number of tabs`() {
        setupToolbarView()
        every { testContext.settings() } returns settings

        val browserState = BrowserState(
            tabs = listOf(
                createTab(url = "https://www.mozilla.org", id = "mozilla"),
                createTab(url = "https://www.firefox.com", id = "firefox"),
                createTab(url = "https://getpocket.com", private = true, id = "getpocket"),
            ),
            selectedTabId = "mozilla",
        )

        tabCounterView.update(browserState)

        verify {
            tabCounter.setCountWithAnimation(browserState.normalTabs.size)
        }

        browsingModeManager.mode = BrowsingMode.Private

        tabCounterView.update(browserState)

        verify {
            tabCounter.setCountWithAnimation(browserState.privateTabs.size)
        }
    }

    @Test
    fun `WHEN state updated while in private mode THEN call toggleCounterMask(true)`() {
        setupToolbarView()
        every { testContext.settings() } returns settings
        val browserState = BrowserState(
            tabs = listOf(
                createTab(url = "https://www.mozilla.org", id = "mozilla"),
                createTab(url = "https://www.firefox.com", id = "firefox"),
                createTab(url = "https://getpocket.com", private = true, id = "getpocket"),
            ),
            selectedTabId = "mozilla",
        )

        browsingModeManager.mode = BrowsingMode.Private
        tabCounterView.update(browserState)

        verifyOrder {
            tabCounter.toggleCounterMask(true)
        }
    }

    @Test
    fun `WHEN state updated while in normal mode THEN call toggleCounterMask(false)`() {
        setupToolbarView()
        every { testContext.settings() } returns settings
        val browserState = BrowserState(
            tabs = listOf(
                createTab(url = "https://www.mozilla.org", id = "mozilla"),
                createTab(url = "https://www.firefox.com", id = "firefox"),
                createTab(url = "https://getpocket.com", private = true, id = "getpocket"),
            ),
            selectedTabId = "mozilla",
        )

        tabCounterView.update(browserState)

        verifyOrder {
            tabCounter.toggleCounterMask(false)
        }
    }

    @Test
    fun `GIVEN the popup menu is enabled WHEN setting up the View THEN set a long press click listener to handle show the menu`() {
        setupToolbarView()

        verify { tabCounter.setOnLongClickListener(any()) }
    }

    @Test
    fun `GIVEN the popup menu is disabled WHEN setting up the View THEN don't set any long press click listeners`() {
        setupToolbarView(showLongPressMenu = false)

        verify(exactly = 0) { tabCounter.setOnLongClickListener(any()) }
    }

    private fun setupToolbarView(
        showLongPressMenu: Boolean = true,
    ) {
        tabCounterView = TabCounterView(
            context = testContext,
            browsingModeManager = browsingModeManager,
            navController = navController,
            tabCounter = tabCounter,
            showLongPressMenu = showLongPressMenu,
        )
    }
}
