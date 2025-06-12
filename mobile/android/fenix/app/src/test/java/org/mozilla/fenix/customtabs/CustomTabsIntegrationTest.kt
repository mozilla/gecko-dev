/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.toolbar.BrowserToolbarView
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

@RunWith(AndroidJUnit4::class)
class CustomTabsIntegrationTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val sessionId = "sessionId"
    private lateinit var browserStore: BrowserStore
    private lateinit var appStore: AppStore
    private lateinit var activity: HomeActivity
    private lateinit var browserToolbarView: BrowserToolbarView
    private lateinit var toolbar: BrowserToolbar
    private lateinit var settings: Settings

    @Before
    fun setup() {
        settings = mockk(relaxed = true)
        every { testContext.components.useCases } returns mockk(relaxed = true)
        every { testContext.components.core } returns mockk(relaxed = true)
        every { testContext.components.publicSuffixList } returns PublicSuffixList(testContext)
        every { testContext.settings() } returns settings

        activity = mockk(relaxed = true)
        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("url", id = sessionId),
                ),
                selectedTabId = sessionId,
            ),
        )
        appStore = AppStore()
        toolbar = BrowserToolbar(testContext).apply {
            id = R.id.toolbar
        }
        browserToolbarView = spyk(
            BrowserToolbarView(
                context = testContext,
                settings = settings,
                container = CoordinatorLayout(testContext).apply {
                    layoutDirection = View.LAYOUT_DIRECTION_LTR
                    addView(toolbar, CoordinatorLayout.LayoutParams(100, 100))
                },
                snackbarParent = mockk(),
                interactor = mockk(),
                customTabSession = mockk(relaxed = true),
                lifecycleOwner = mockk(),
                tabStripContent = {},
            ),
        )

        toolbar = spyk(toolbar)
        browserToolbarView.toolbar = toolbar
        every { activity.settings() } returns settings
    }

    @Test
    fun `WHEN initOpenInAction is called THEN openInAction is initialized and added to the toolbar`() {
        val integration = spyk(getIntegration())

        assertNull(integration.openInAction)

        integration.initOpenInAction(testContext, 0, 0)

        assertNotNull(integration.openInAction)
        verify { toolbar.addBrowserAction(any()) }
    }

    @Test
    fun `WHEN removeOpenInAction is called THEN openInAction is removed from the toolbar and equals null`() {
        val integration = spyk(getIntegration()).apply {
            openInAction = mockk()
        }

        assertNotNull(integration.openInAction)

        integration.removeOpenInAction()

        assertNull(integration.openInAction)
        verify { toolbar.removeBrowserAction(any()) }
    }

    @Test
    fun `WHEN initForwardAction is called THEN forwardAction is initialized and added to the toolbar`() {
        val integration = spyk(getIntegration())

        assertNull(integration.forwardAction)

        integration.initForwardAction(testContext, 0, 0)

        assertNotNull(integration.forwardAction)
        verify(exactly = 1) { toolbar.addNavigationAction(any()) }
    }

    @Test
    fun `WHEN initBackwardAction is called THEN backAction is initialized and added to the toolbar`() {
        val integration = spyk(getIntegration())

        assertNull(integration.backAction)

        integration.initBackwardAction(testContext, 0, 0)

        assertNotNull(integration.backAction)
        verify(exactly = 1) { toolbar.addNavigationAction(any()) }
    }

    @Test
    fun `WHEN addNavigationActions is called THEN initBackwardAction and initForwardAction are called`() {
        val integration = spyk(getIntegration())

        integration.addNavigationActions(testContext)

        verify { integration.initForwardAction(any(), any(), any()) }
        verify { integration.initBackwardAction(any(), any(), any()) }
    }

    @Test
    fun `WHEN removeNavigationActions is called THEN  backAction and forwardAction are removed from the toolbar and equal null`() {
        val integration = spyk(getIntegration()).apply {
            forwardAction = mockk()
            backAction = mockk()
        }

        assertNotNull(integration.forwardAction)
        assertNotNull(integration.backAction)

        integration.removeNavigationActions()

        assertNull(integration.forwardAction)
        assertNull(integration.backAction)
        verify(exactly = 2) { toolbar.removeNavigationAction(any()) }
    }

    private fun getIntegration(
        toolbar: BrowserToolbar = this.toolbar,
    ): CustomTabsIntegration {
        return CustomTabsIntegration(
            context = testContext,
            store = browserStore,
            interactor = mockk(),
            useCases = mockk(),
            browserToolbar = toolbar,
            sessionId = sessionId,
            activity = activity,
            isPrivate = false,
            shouldReverseItems = false,
            isSandboxCustomTab = false,
            isMenuRedesignEnabled = false,
        )
    }
}
