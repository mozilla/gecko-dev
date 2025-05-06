/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.RestoreCompleteAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.usecases.FenixBrowserUseCases
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

@RunWith(AndroidJUnit4::class)
class StartupMiddlewareTest {
    private lateinit var useCases: FenixBrowserUseCases
    private lateinit var settings: Settings
    private lateinit var repository: HomepageAsANewTabPreferencesRepository
    private lateinit var middleware: StartupMiddleware
    private lateinit var captureActionsMiddleware: CaptureActionsMiddleware<BrowserState, BrowserAction>

    @Before
    fun setup() {
        useCases = mockk(relaxed = true)
        settings = Settings(testContext)
        repository = DefaultHomepageAsANewTabPreferenceRepository(settings)

        captureActionsMiddleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        middleware = StartupMiddleware(applicationContext = testContext, repository = repository)

        every { testContext.components.useCases.fenixBrowserUseCases } returns useCases
    }

    @Test
    fun `GIVEN homepage as a new tab is enabled and no tabs to be restored WHEN restore complete action is dispatched THEN add a new homepage tab`() {
        settings.enableHomepageAsNewTab = true
        val store = createStore()

        store.dispatch(RestoreCompleteAction).joinBlocking()

        captureActionsMiddleware.assertLastAction(RestoreCompleteAction::class) {}

        verify {
            useCases.addNewHomepageTab(private = false)
        }
    }

    @Test
    fun `GIVEN homepage as a new tab is disabled no tabs to be restored WHEN restore complete action is dispatched THEN do nothing`() {
        settings.enableHomepageAsNewTab = false
        val store = createStore()

        store.dispatch(RestoreCompleteAction).joinBlocking()

        captureActionsMiddleware.assertLastAction(RestoreCompleteAction::class) {}

        verify(exactly = 0) {
            useCases.addNewHomepageTab(private = false)
        }
    }

    @Test
    fun `GIVEN homepage as a new tab is enabled and a tab was restored WHEN restore complete action is dispatched THEN do nothing`() {
        settings.enableHomepageAsNewTab = true
        val tab = createTab("https://www.mozilla.org", id = "test-tab1")
        val store = createStore(
            initialState = BrowserState(tabs = listOf(tab)),
        )

        store.dispatch(RestoreCompleteAction).joinBlocking()

        captureActionsMiddleware.assertLastAction(RestoreCompleteAction::class) {}

        verify(exactly = 0) {
            useCases.addNewHomepageTab(private = false)
        }
    }

    private fun createStore(
        initialState: BrowserState = BrowserState(),
    ) = BrowserStore(
        initialState = initialState,
        middleware = listOf(middleware, captureActionsMiddleware),
    )
}
