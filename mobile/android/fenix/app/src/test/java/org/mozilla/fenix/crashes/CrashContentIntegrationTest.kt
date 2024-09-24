/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import android.content.res.Configuration
import android.view.ViewGroup.MarginLayoutParams
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.OrientationMode
import org.mozilla.fenix.utils.Settings

@RunWith(AndroidJUnit4::class)
class CrashContentIntegrationTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val sessionId = "sessionId"
    private lateinit var browserStore: BrowserStore
    private lateinit var appStore: AppStore
    private lateinit var settings: Settings

    @Before
    fun setup() {
        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("url", id = sessionId),
                ),
                selectedTabId = sessionId,
            ),
        )
        appStore = AppStore()
        settings = mockk {
            every { getBottomToolbarHeight(any()) } returns 100
            every { getTopToolbarHeight(any()) } returns 100
        }
    }

    @Test
    fun `GIVEN a tab WHEN its content crashes THEN expand the toolbar and show the in-content crash reporter`() {
        val crashReporterLayoutParams: MarginLayoutParams = mockk(relaxed = true)
        val crashReporterView: CrashContentView = mockk(relaxed = true) {
            every { layoutParams } returns crashReporterLayoutParams
        }
        val toolbar: BrowserToolbar = mockk(relaxed = true) {
            every { height } returns 33
        }
        val components: Components = mockk()
        val integration = CrashContentIntegration(
            context = testContext,
            browserStore = browserStore,
            appStore = appStore,
            toolbar = toolbar,
            crashReporterView = crashReporterView,
            components = components,
            settings = settings,
            navController = mockk(),
            sessionId = sessionId,
        )
        val controllerCaptor = slot<CrashReporterController>()
        integration.start()
        browserStore.dispatch(CrashAction.SessionCrashedAction(sessionId))
        coroutinesTestRule.testDispatcher.scheduler.advanceUntilIdle()

        verify {
            toolbar.expand()
            crashReporterLayoutParams.topMargin = 33
            crashReporterView.show(capture(controllerCaptor))
        }
        assertEquals(sessionId, controllerCaptor.captured.sessionId)
        assertEquals(components, controllerCaptor.captured.components)
        assertEquals(settings, controllerCaptor.captured.settings)
        assertEquals(appStore, controllerCaptor.captured.appStore)
    }

    @Test
    fun `GIVEN a tab is marked as crashed WHEN the crashed state changes THEN hide the in-content crash reporter`() {
        val crashReporterLayoutParams: MarginLayoutParams = mockk(relaxed = true)
        val crashReporterView: CrashContentView = mockk(relaxed = true) {
            every { layoutParams } returns crashReporterLayoutParams
        }
        val integration = CrashContentIntegration(
            context = testContext,
            browserStore = browserStore,
            appStore = appStore,
            toolbar = mockk(),
            crashReporterView = crashReporterView,
            components = mockk(),
            settings = settings,
            navController = mockk(),
            sessionId = sessionId,
        )

        integration.start()
        browserStore.dispatch(CrashAction.RestoreCrashedSessionAction(sessionId))
        coroutinesTestRule.testDispatcher.scheduler.advanceUntilIdle()

        verify { crashReporterView.hide() }
    }

    @Test
    fun `WHEN orientation state changes THEN margins are updated`() {
        val crashReporterLayoutParams: MarginLayoutParams = mockk(relaxed = true)
        val crashReporterView: CrashContentView = mockk(relaxed = true) {
            every { layoutParams } returns crashReporterLayoutParams
        }
        val integration = spyk(
            CrashContentIntegration(
                context = testContext,
                browserStore = browserStore,
                appStore = appStore,
                toolbar = mockk(),
                crashReporterView = crashReporterView,
                components = mockk(),
                settings = settings,
                navController = mockk(),
                sessionId = sessionId,
            ),
        )

        integration.start()
        appStore.dispatch(AppAction.OrientationChange(orientation = OrientationMode.fromInteger(Configuration.ORIENTATION_LANDSCAPE)))
        coroutinesTestRule.testDispatcher.scheduler.advanceUntilIdle()

        verify { integration.updateVerticalMargins() }
    }

    @Test
    fun `GIVEN integration was stopped and then restarted WHEN orientation state changes THEN margins are updated`() {
        val crashReporterLayoutParams: MarginLayoutParams = mockk(relaxed = true)
        val crashReporterView: CrashContentView = mockk(relaxed = true) {
            every { layoutParams } returns crashReporterLayoutParams
        }
        val integration = spyk(
            CrashContentIntegration(
                context = testContext,
                browserStore = browserStore,
                appStore = appStore,
                toolbar = mockk(),
                crashReporterView = crashReporterView,
                components = mockk(),
                settings = settings,
                navController = mockk(),
                sessionId = sessionId,
            ),
        )
        val scopeTwo = TestScope()
        integration.scope = scopeTwo

        integration.start()
        integration.stop()
        integration.start()
        appStore.dispatch(AppAction.OrientationChange(orientation = OrientationMode.fromInteger(Configuration.ORIENTATION_LANDSCAPE)))
        coroutinesTestRule.testDispatcher.scheduler.advanceUntilIdle()
        scopeTwo.advanceUntilIdle()

        verify { integration.updateVerticalMargins() }
    }
}
