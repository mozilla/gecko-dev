/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.spyk
import io.mockk.unmockkStatic
import io.mockk.verify
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.ui.widgets.VerticalSwipeRefreshLayout
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.tabstrip.isTabStripEnabled
import org.mozilla.fenix.components.FindInPageIntegration
import org.mozilla.fenix.components.toolbar.navbar.EngineViewClippingBehavior
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.isKeyboardVisible
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings
import kotlin.reflect.KFunction

class BaseBrowserFragmentTest {
    private lateinit var fragment: TestBaseBrowserFragment
    private lateinit var swipeRefreshLayout: VerticalSwipeRefreshLayout
    private lateinit var engineView: EngineView
    private lateinit var settings: Settings
    private lateinit var testContext: Context

    @Before
    fun setup() {
        fragment = spyk(TestBaseBrowserFragment())
        swipeRefreshLayout = mockk(relaxed = true)
        engineView = mockk(relaxed = true)
        settings = mockk(relaxed = true)
        testContext = mockk(relaxed = true)

        every { testContext.components.settings } returns settings
        every { testContext.settings() } returns settings
        every { fragment.isAdded } returns true
        every { fragment.activity } returns mockk()
        every { fragment.requireContext() } returns testContext
        every { fragment.getEngineView() } returns engineView
        every { fragment.getSwipeRefreshLayout() } returns swipeRefreshLayout
        every { swipeRefreshLayout.layoutParams } returns mockk<CoordinatorLayout.LayoutParams>(relaxed = true)
        every { fragment.binding } returns mockk(relaxed = true)
        every { fragment.viewLifecycleOwner } returns mockk(relaxed = true)
    }

    @Test
    fun `initializeEngineView should setDynamicToolbarMaxHeight to 0 if top toolbar is forced for a11y`() {
        every { settings.shouldUseBottomToolbar } returns false
        every { settings.shouldUseFixedTopToolbar } returns true

        fragment.initializeEngineView(
            topToolbarHeight = 13,
            bottomToolbarHeight = 0,
        )

        verify { engineView.setDynamicToolbarMaxHeight(0) }
    }

    @Test
    fun `initializeEngineView should setDynamicToolbarMaxHeight to 0 if bottom toolbar is forced for a11y`() {
        every { settings.shouldUseBottomToolbar } returns true
        every { settings.shouldUseFixedTopToolbar } returns true

        fragment.initializeEngineView(
            topToolbarHeight = 13,
            bottomToolbarHeight = 13,
        )

        verify { engineView.setDynamicToolbarMaxHeight(0) }
    }

    @Test
    fun `initializeEngineView should setDynamicToolbarMaxHeight to toolbar height if dynamic toolbar is enabled`() {
        every { settings.shouldUseFixedTopToolbar } returns false
        every { settings.isDynamicToolbarEnabled } returns true
        every { settings.navigationToolbarEnabled } returns true

        fragment.initializeEngineView(
            topToolbarHeight = 13,
            bottomToolbarHeight = 0,
        )

        verify { engineView.setDynamicToolbarMaxHeight(13) }
    }

    @Test
    fun `initializeEngineView should setDynamicToolbarMaxHeight to 0 if dynamic toolbar is disabled`() {
        every { settings.shouldUseFixedTopToolbar } returns false
        every { settings.isDynamicToolbarEnabled } returns false

        fragment.initializeEngineView(
            topToolbarHeight = 13,
            bottomToolbarHeight = 0,
        )

        verify { engineView.setDynamicToolbarMaxHeight(0) }
    }

    @Test
    fun `initializeEngineView should set EngineViewClippingBehavior when dynamic toolbar is enabled`() {
        every { settings.shouldUseFixedTopToolbar } returns false
        every { settings.isDynamicToolbarEnabled } returns true
        every { settings.navigationToolbarEnabled } returns true
        val params: CoordinatorLayout.LayoutParams = mockk(relaxed = true)
        every { params.behavior } returns mockk(relaxed = true)
        every { swipeRefreshLayout.layoutParams } returns params
        val behavior = slot<EngineViewClippingBehavior>()

        fragment.initializeEngineView(
            topToolbarHeight = 13,
            bottomToolbarHeight = 0,
        )

        // EngineViewClippingBehavior constructor parameters are not properties, we cannot check them.
        // Ensure just that the right behavior is set.
        verify { params.behavior = capture(behavior) }
    }

    @Test
    fun `initializeEngineView should set toolbar height as EngineView parent's bottom margin when using bottom toolbar`() {
        every { settings.isDynamicToolbarEnabled } returns false
        every { settings.shouldUseBottomToolbar } returns true

        fragment.initializeEngineView(
            topToolbarHeight = 0,
            bottomToolbarHeight = 13,
        )

        verify { (swipeRefreshLayout.layoutParams as CoordinatorLayout.LayoutParams).bottomMargin = 13 }
    }

    @Test
    fun `initializeEngineView should set toolbar height as EngineView parent's bottom margin if top toolbar is forced for a11y`() {
        every { settings.shouldUseBottomToolbar } returns false
        every { settings.shouldUseFixedTopToolbar } returns true

        fragment.initializeEngineView(
            topToolbarHeight = 13,
            bottomToolbarHeight = 0,
        )

        verify { (swipeRefreshLayout.layoutParams as CoordinatorLayout.LayoutParams).bottomMargin = 13 }
    }

    @Test
    fun `initializeEngineView should set toolbar height as EngineView parent's bottom margin if bottom toolbar is forced for a11y`() {
        every { settings.shouldUseBottomToolbar } returns true
        every { settings.shouldUseFixedTopToolbar } returns true

        fragment.initializeEngineView(
            topToolbarHeight = 0,
            bottomToolbarHeight = 13,
        )

        verify { (swipeRefreshLayout.layoutParams as CoordinatorLayout.LayoutParams).bottomMargin = 13 }
    }

    @Test
    fun `WHEN status is equals to FAILED or COMPLETED and it is the same tab then shouldShowCompletedDownloadDialog will be true`() {
        every { fragment.getCurrentTab() } returns createTab(id = "1", url = "")

        val download = DownloadState(
            url = "",
            sessionId = "1",
            destinationDirectory = "/",
            directoryPath = "/",
        )

        val status = DownloadState.Status.entries
            .filter { it == DownloadState.Status.COMPLETED && it == DownloadState.Status.FAILED }

        status.forEach {
            val result =
                fragment.shouldShowCompletedDownloadDialog(download, it)

            assertTrue(result)
        }
    }

    @Test
    fun `WHEN status is different from FAILED or COMPLETED then shouldShowCompletedDownloadDialog will be false`() {
        every { fragment.getCurrentTab() } returns createTab(id = "1", url = "")

        val download = DownloadState(
            url = "",
            sessionId = "1",
            destinationDirectory = "/",
            directoryPath = "/",
        )

        val status = DownloadState.Status.entries
            .filter { it != DownloadState.Status.COMPLETED && it != DownloadState.Status.FAILED }

        status.forEach {
            val result =
                fragment.shouldShowCompletedDownloadDialog(download, it)

            assertFalse(result)
        }
    }

    @Test
    fun `WHEN the tab is different from the initial one then shouldShowCompletedDownloadDialog will be false`() {
        every { fragment.getCurrentTab() } returns createTab(id = "1", url = "")

        val download = DownloadState(
            url = "",
            sessionId = "2",
            destinationDirectory = "/",
            directoryPath = "/",
        )

        val status = DownloadState.Status.entries
            .filter { it != DownloadState.Status.COMPLETED && it != DownloadState.Status.FAILED }

        status.forEach {
            val result =
                fragment.shouldShowCompletedDownloadDialog(download, it)

            assertFalse(result)
        }
    }

    @Test
    fun `WHEN initializeEngineView is called  THEN setDynamicToolbarMaxHeight sets max height to the engine view as a sum of two toolbars heights`() {
        every { settings.shouldUseFixedTopToolbar } returns false
        every { settings.isDynamicToolbarEnabled } returns true
        every { settings.navigationToolbarEnabled } returns true

        fragment.initializeEngineView(
            topToolbarHeight = 13,
            bottomToolbarHeight = 0,
        )
        verify { engineView.setDynamicToolbarMaxHeight(13) }

        fragment.initializeEngineView(
            topToolbarHeight = 0,
            bottomToolbarHeight = 13,
        )
        verify { engineView.setDynamicToolbarMaxHeight(13) }

        fragment.initializeEngineView(
            topToolbarHeight = 13,
            bottomToolbarHeight = 13,
        )
        verify { engineView.setDynamicToolbarMaxHeight(26) }
    }

    @Test
    fun `WHEN isMicrosurveyEnabled and isExperimentationEnabled are true GIVEN a call to setupMicrosurvey THEN messagingFeature is initialized`() {
        every { testContext.settings().isExperimentationEnabled } returns true
        every { testContext.settings().microsurveyFeatureEnabled } returns true

        assertNull(fragment.messagingFeatureMicrosurvey.get())

        fragment.initializeMicrosurveyFeature(testContext)

        assertNotNull(fragment.messagingFeatureMicrosurvey.get())
    }

    @Test
    fun `WHEN isMicrosurveyEnabled and isExperimentationEnabled are false GIVEN a call to setupMicrosurvey THEN messagingFeature is not initialized`() {
        every { testContext.settings().isExperimentationEnabled } returns false
        every { testContext.settings().microsurveyFeatureEnabled } returns false

        assertNull(fragment.messagingFeatureMicrosurvey.get())

        fragment.initializeMicrosurveyFeature(testContext)

        assertNull(fragment.messagingFeatureMicrosurvey.get())
    }

    @Test
    fun `WHEN isMicrosurveyEnabled is true and isExperimentationEnabled false GIVEN a call to setupMicrosurvey THEN messagingFeature is not initialized`() {
        every { testContext.settings().isExperimentationEnabled } returns false
        every { testContext.settings().microsurveyFeatureEnabled } returns true

        assertNull(fragment.messagingFeatureMicrosurvey.get())

        fragment.initializeMicrosurveyFeature(testContext)

        assertNull(fragment.messagingFeatureMicrosurvey.get())
    }

    @Test
    fun `WHEN isMicrosurveyEnabled is false and isExperimentationEnabled true GIVEN a call to setupMicrosurvey THEN messagingFeature is not initialized`() {
        every { testContext.settings().isExperimentationEnabled } returns true
        every { testContext.settings().microsurveyFeatureEnabled } returns false

        assertNull(fragment.messagingFeatureMicrosurvey.get())

        fragment.initializeMicrosurveyFeature(testContext)

        assertNull(fragment.messagingFeatureMicrosurvey.get())
    }

    @Test
    fun `GIVEN fixed toolbar WHEN setting engine view insets THEN set none`() {
        val currentTab = createTab("https://example.com")
        every { testContext.components.core.store.state } returns BrowserState(
            tabs = listOf(currentTab),
            selectedTabId = currentTab.id,
        )
        every { fragment.view } returns mockk {
            every { context } returns testContext
        }

        fragment.configureEngineViewWithDynamicToolbarsMaxHeight()

        verify { engineView.setDynamicToolbarMaxHeight(0) }
    }

    @Test
    fun `GIVEN only a top toolbar WHEN setting engine view insets THEN use top toolbar's height`() {
        val currentTab = createTab("https://example.com")
        every { testContext.components.core.store.state } returns BrowserState(
            tabs = listOf(currentTab),
            selectedTabId = currentTab.id,
        )
        every { fragment.view } returns mockk {
            every { context } returns testContext
        }
        every { settings.getTopToolbarHeight(any()) } returns 11
        every { settings.getBottomToolbarHeight(any()) } returns 0
        every { settings.isDynamicToolbarEnabled } returns true

        safeMockkStatic(
            View::isKeyboardVisible,
            Context::isTabStripEnabled,
        ) {
            every { any<View>().isKeyboardVisible() } returns false
            every { testContext.isTabStripEnabled() } returns false

            fragment.configureEngineViewWithDynamicToolbarsMaxHeight()
        }

        verify { engineView.setDynamicToolbarMaxHeight(11) }
    }

    @Test
    fun `GIVEN a pdf shown with a dynamic top toolbar WHEN setting engine view insets THEN set none`() {
        var currentTab = createTab("https://example.com")
        currentTab = currentTab.copy(
            content = currentTab.content.copy(
                isPdf = true,
            ),
        )
        every { testContext.components.core.store.state } returns BrowserState(
            tabs = listOf(currentTab),
            selectedTabId = currentTab.id,
        )
        every { fragment.view } returns mockk {
            every { context } returns testContext
        }
        every { settings.getTopToolbarHeight(any()) } returns 11
        every { settings.getBottomToolbarHeight(any()) } returns 0
        every { settings.isDynamicToolbarEnabled } returns true

        safeMockkStatic(
            View::isKeyboardVisible,
            Context::isTabStripEnabled,
        ) {
            every { any<View>().isKeyboardVisible() } returns false
            every { testContext.isTabStripEnabled() } returns false

            fragment.configureEngineViewWithDynamicToolbarsMaxHeight()
        }

        verify(exactly = 0) { engineView.setDynamicToolbarMaxHeight(any()) }
    }

    @Test
    fun `GIVEN find in page active with a dynamic top toolbar WHEN setting engine view insets THEN set none`() {
        val currentTab = createTab("https://example.com")
        every { testContext.components.core.store.state } returns BrowserState(
            tabs = listOf(currentTab),
            selectedTabId = currentTab.id,
        )
        val findInPageIntegration: FindInPageIntegration = mockk {
            every { isFeatureActive } returns true
        }
        fragment.findInPageIntegration.set(
            feature = findInPageIntegration,
            owner = mockk(relaxed = true),
            view = mockk(relaxed = true),
        )
        every { fragment.view } returns mockk {
            every { context } returns testContext
        }
        every { settings.getTopToolbarHeight(any()) } returns 11
        every { settings.getBottomToolbarHeight(any()) } returns 0
        every { settings.isDynamicToolbarEnabled } returns true

        safeMockkStatic(
            View::isKeyboardVisible,
            Context::isTabStripEnabled,
        ) {
            every { any<View>().isKeyboardVisible() } returns false
            every { testContext.isTabStripEnabled() } returns false

            fragment.configureEngineViewWithDynamicToolbarsMaxHeight()
        }

        verify(exactly = 0) { engineView.setDynamicToolbarMaxHeight(any()) }
    }

    @Test
    fun `GIVEN only a bottom toolbar WHEN setting engine view insets THEN use bottom toolbar's height`() {
        val currentTab = createTab("https://example.com")
        every { testContext.components.core.store.state } returns BrowserState(
            tabs = listOf(currentTab),
            selectedTabId = currentTab.id,
        )
        every { fragment.view } returns mockk {
            every { context } returns testContext
        }
        every { settings.getTopToolbarHeight(any()) } returns 0
        every { settings.getBottomToolbarHeight(any()) } returns 22
        every { settings.isDynamicToolbarEnabled } returns true

        safeMockkStatic(
            View::isKeyboardVisible,
            Context::isTabStripEnabled,
        ) {
            every { any<View>().isKeyboardVisible() } returns false
            every { testContext.isTabStripEnabled() } returns false

            fragment.configureEngineViewWithDynamicToolbarsMaxHeight()
        }

        verify { engineView.setDynamicToolbarMaxHeight(22) }
    }

    @Test
    fun `GIVEN addressbar and navbar shown WHEN setting engine view insets THEN use both toolbars' heights`() {
        val currentTab = createTab("https://example.com")
        every { testContext.components.core.store.state } returns BrowserState(
            tabs = listOf(currentTab),
            selectedTabId = currentTab.id,
        )
        every { fragment.view } returns mockk {
            every { context } returns testContext
        }
        every { settings.getTopToolbarHeight(any()) } returns 11
        every { settings.getBottomToolbarHeight(any()) } returns 22
        every { settings.isDynamicToolbarEnabled } returns true
        every { testContext.resources.getDimensionPixelSize(R.dimen.browser_navbar_height) } returns 10

        safeMockkStatic(
            View::isKeyboardVisible,
            Context::isTabStripEnabled,
        ) {
            every { any<View>().isKeyboardVisible() } returns false
            every { testContext.isTabStripEnabled() } returns false

            fragment.configureEngineViewWithDynamicToolbarsMaxHeight()
        }

        verify { engineView.setDynamicToolbarMaxHeight(33) }
    }

    @Test
    fun `GIVEN keyboard shown WHEN setting engine view insets THEN use both toolbars' heights`() {
        val currentTab = createTab("https://example.com")
        every { testContext.components.core.store.state } returns BrowserState(
            tabs = listOf(currentTab),
            selectedTabId = currentTab.id,
        )
        every { fragment.view } returns mockk {
            every { context } returns testContext
        }
        every { settings.getTopToolbarHeight(any()) } returns 11
        every { settings.getBottomToolbarHeight(any()) } returns 22
        every { settings.isDynamicToolbarEnabled } returns true
        every { testContext.resources.getDimensionPixelSize(R.dimen.browser_navbar_height) } returns 10

        safeMockkStatic(
            View::isKeyboardVisible,
            Context::isTabStripEnabled,
        ) {
            every { any<View>().isKeyboardVisible() } returns true
            every { testContext.isTabStripEnabled() } returns false

            fragment.configureEngineViewWithDynamicToolbarsMaxHeight()
        }

        verify { engineView.setDynamicToolbarMaxHeight(23) }
    }
}

private inline fun safeMockkStatic(vararg objects: KFunction<*>, block: () -> Unit) {
    try {
        mockkStatic(*objects)
        block()
    } finally {
        unmockkStatic(*objects)
    }
}

private class TestBaseBrowserFragment : BaseBrowserFragment() {
    override fun getContextMenuCandidates(
        context: Context,
        view: View,
    ): List<ContextMenuCandidate> {
        // no-op
        return emptyList()
    }

    override fun navToQuickSettingsSheet(tab: SessionState, sitePermissions: SitePermissions?) {
        // no-op
    }
}
