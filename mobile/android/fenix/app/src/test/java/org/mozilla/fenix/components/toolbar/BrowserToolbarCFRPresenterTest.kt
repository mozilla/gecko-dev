/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.view.View
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CookieBannerAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.After
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.tabstrip.isTabStripEnabled
import org.mozilla.fenix.ext.isLargeWindow
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class BrowserToolbarCFRPresenterTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Before
    fun setup() {
        mockkStatic("org.mozilla.fenix.ext.ContextKt")
        mockkStatic(Context::isTabStripEnabled)
    }

    @After
    fun teardown() {
        unmockkStatic("org.mozilla.fenix.ext.ContextKt")
        unmockkStatic(Context::isTabStripEnabled)
    }

    @Test
    fun `GIVEN the cookie banners handling CFR should be shown for a custom tab WHEN the custom tab is fully loaded THEN the TCP CFR is shown`() {
        val privateTab = createTab(url = "", private = true)
        val browserStore = createBrowserStore(tab = privateTab, selectedTabId = privateTab.id)
        val settings: Settings = mockk(relaxed = true) {
            every { reviewQualityCheckOptInTimeInMillis } returns System.currentTimeMillis()
            every { shouldShowEraseActionCFR } returns false
            every { shouldShowCookieBannersCFR } returns true
            every { shouldUseCookieBannerPrivateMode } returns true
        }
        val presenter = createPresenter(
            isPrivate = true,
            browserStore = browserStore,
            settings = settings,
        )

        presenter.start()

        assertNotNull(presenter.scope)

        browserStore.dispatch(
            CookieBannerAction.UpdateStatusAction(
                privateTab.id,
                EngineSession.CookieBannerHandlingStatus.HANDLED,
            ),
        ).joinBlocking()

        verify { presenter.showCookieBannersCFR() }
        verify { settings.shouldShowCookieBannersCFR = false }
    }

    @Test
    fun `GIVEN the Erase CFR should be shown WHEN in private mode and the current tab is fully loaded THEN the Erase CFR is only shown once`() {
        val tab = createTab(url = "", private = true)

        val browserStore = createBrowserStore(
            tab = tab,
            selectedTabId = tab.id,
        )

        val presenter = createPresenterThatShowsCFRs(
            browserStore = browserStore,
            settings = mockk {
                every { shouldShowTabSwipeCFR } returns false
                every { hasShownTabSwipeCFR } returns false
                every { shouldShowEraseActionCFR } returns true
            },
            isPrivate = true,
        )

        presenter.start()

        assertNotNull(presenter.scope)

        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 99)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        browserStore.dispatch(ContentAction.UpdateProgressAction(tab.id, 100)).joinBlocking()
        verify { presenter.showEraseCfr() }
    }

    @Test
    fun `GIVEN the store is observed for updates WHEN the presenter is stopped THEN stop observing the store`() {
        val scope: CoroutineScope = mockk {
            every { cancel() } just Runs
        }
        val presenter = createPresenter()
        presenter.scope = scope

        presenter.stop()

        verify { scope.cancel() }
    }

    @Test
    fun `GIVEN the Tab Swipe CFR should be shown WHEN in Normal mode THEN the Tab Swipe CFR is shown once`() {
        val normalTab = createTab(url = "", private = false)
        val browserStore = createBrowserStore(tab = normalTab, selectedTabId = normalTab.id)
        val context: Context = mockk {
            every { isTabStripEnabled() } returns false
        }
        val settings: Settings = mockk(relaxed = true) {
            every { reviewQualityCheckOptInTimeInMillis } returns System.currentTimeMillis()
            every { shouldShowEraseActionCFR } returns false
            every { shouldShowCookieBannersCFR } returns false
            every { shouldUseCookieBannerPrivateMode } returns false
            every { shouldShowTabSwipeCFR } returns true
            every { isSwipeToolbarToSwitchTabsEnabled } returns true
            every { hasShownTabSwipeCFR } returns false
        }

        val presenter = createPresenterThatShowsCFRs(
            context = context,
            browserStore = browserStore,
            settings = settings,
            isPrivate = false,
        )

        presenter.start()

        verify { presenter.showTabSwipeCFR() }
        verify { settings.hasShownTabSwipeCFR = true }
        verify { settings.shouldShowTabSwipeCFR = false }
    }

    @Test
    fun `GIVEN tab strip is enabled WHEN in Normal mode THEN the Tab Swipe CFR is not shown`() {
        val normalTab = createTab(url = "", private = false)
        val browserStore = createBrowserStore(tab = normalTab, selectedTabId = normalTab.id)
        val context: Context = mockk {
            every { isTabStripEnabled() } returns true
        }
        val settings: Settings = mockk(relaxed = true) {
            every { reviewQualityCheckOptInTimeInMillis } returns System.currentTimeMillis()
            every { shouldShowEraseActionCFR } returns false
            every { shouldShowCookieBannersCFR } returns false
            every { shouldUseCookieBannerPrivateMode } returns false
            every { shouldShowTabSwipeCFR } returns true
            every { isSwipeToolbarToSwitchTabsEnabled } returns true
            every { hasShownTabSwipeCFR } returns false
        }

        val presenter = createPresenterThatShowsCFRs(
            context = context,
            browserStore = browserStore,
            settings = settings,
            isPrivate = false,
        )

        presenter.start()

        verify(exactly = 0) { presenter.showTabSwipeCFR() }
        verify(exactly = 0) { settings.hasShownTabSwipeCFR = any() }
        verify(exactly = 0) { settings.shouldShowTabSwipeCFR = any() }
    }

    @Test
    fun `GIVEN swipe toolbar to change tabs is disabled WHEN in Normal mode THEN the Tab Swipe CFR is not shown`() {
        val normalTab = createTab(url = "", private = false)
        val browserStore = createBrowserStore(tab = normalTab, selectedTabId = normalTab.id)
        val context: Context = mockk {
            every { isTabStripEnabled() } returns false
        }
        val settings: Settings = mockk(relaxed = true) {
            every { reviewQualityCheckOptInTimeInMillis } returns System.currentTimeMillis()
            every { shouldShowEraseActionCFR } returns false
            every { shouldShowCookieBannersCFR } returns false
            every { shouldUseCookieBannerPrivateMode } returns false
            every { shouldShowTabSwipeCFR } returns true
            every { isSwipeToolbarToSwitchTabsEnabled } returns false
            every { hasShownTabSwipeCFR } returns false
        }

        val presenter = createPresenterThatShowsCFRs(
            context = context,
            browserStore = browserStore,
            settings = settings,
            isPrivate = false,
        )

        presenter.start()

        verify(exactly = 0) { presenter.showTabSwipeCFR() }
        verify(exactly = 0) { settings.hasShownTabSwipeCFR = any() }
        verify(exactly = 0) { settings.shouldShowTabSwipeCFR = any() }
    }

    /**
     * Creates and return a [spyk] of a [BrowserToolbarCFRPresenter] that can handle actually showing CFRs.
     */
    private fun createPresenterThatShowsCFRs(
        context: Context = mockk {
            every { isLargeWindow() } returns false
        },
        anchor: View = mockk(),
        browserStore: BrowserStore = mockk(),
        settings: Settings = mockk {
            every { openTabsCount } returns 5
            every { shouldShowEraseActionCFR } returns false
        },
        toolbar: BrowserToolbar = mockk(),
        isPrivate: Boolean = false,
        sessionId: String? = null,
    ) = spyk(createPresenter(context, anchor, browserStore, settings, toolbar, sessionId, isPrivate)) {
        every { showEraseCfr() } just Runs
    }

    /**
     * Create and return a [BrowserToolbarCFRPresenter] with all constructor properties mocked by default.
     * Calls to show a CFR will fail. If this behavior is needed to work use [createPresenterThatShowsCFRs].
     */
    private fun createPresenter(
        context: Context = mockk {
            every { getColor(any()) } returns 0
        },
        anchor: View = mockk(relaxed = true),
        browserStore: BrowserStore = mockk(),
        settings: Settings = mockk(relaxed = true) {
            every { shouldShowEraseActionCFR } returns true
            every { openTabsCount } returns 5
            every { shouldShowCookieBannersCFR } returns true
            every { shouldShowTabSwipeCFR } returns false
            every { hasShownTabSwipeCFR } returns false
        },
        toolbar: BrowserToolbar = mockk {
            every { findViewById<View>(R.id.mozac_browser_toolbar_security_indicator) } returns anchor
            every { findViewById<View>(R.id.mozac_browser_toolbar_page_actions) } returns anchor
            every { findViewById<View>(R.id.mozac_browser_toolbar_navigation_actions) } returns anchor
        },
        sessionId: String? = null,
        isPrivate: Boolean = false,
    ) = spyk(
        BrowserToolbarCFRPresenter(
            context = context,
            browserStore = browserStore,
            settings = settings,
            toolbar = toolbar,
            customTabId = sessionId,
            isPrivate = isPrivate,
        ),
    )

    private fun createBrowserStore(
        tab: TabSessionState? = null,
        customTab: CustomTabSessionState? = null,
        selectedTabId: String? = null,
    ) = BrowserStore(
        initialState = BrowserState(
            tabs = if (tab != null) listOf(tab) else listOf(),
            customTabs = if (customTab != null) listOf(customTab) else listOf(),
            selectedTabId = selectedTabId,
        ),
    )
}
