/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.settings

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.components.browser.state.action.DefaultDesktopModeAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.desktopmode.DesktopModeMiddleware
import org.mozilla.fenix.browser.desktopmode.DesktopModeRepository

@RunWith(AndroidJUnit4::class)
class DesktopModeMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `GIVEN desktop mode is enabled WHEN the Store is initialized THEN the middleware should set the correct value in the Store`() = runTestOnMain {
        val expected = true
        val middleware = createMiddleware(
            scope = this,
            getDesktopBrowsingEnabled = { true },
        )
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(middleware),
        )

        advanceUntilIdle()
        store.waitUntilIdle()

        launch {
            assertEquals(expected, store.state.desktopMode)
        }
    }

    @Test
    fun `GIVEN desktop mode is disabled WHEN the Store is initialized THEN the middleware should set the correct value in the Store`() = runTestOnMain {
        val expected = false
        val middleware = createMiddleware(
            scope = this,
            getDesktopBrowsingEnabled = { expected },
        )
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(middleware),
        )

        advanceUntilIdle()
        store.waitUntilIdle()

        launch {
            assertEquals(expected, store.state.desktopMode)
        }
    }

    @Test
    fun `GIVEN desktop mode is enabled WHEN the user toggles desktop mode off THEN the preference is updated`() = runTestOnMain {
        val expected = false
        val middleware = createMiddleware(
            scope = this,
            getDesktopBrowsingEnabled = { true },
            updateDesktopBrowsingEnabled = {
                assertEquals(expected, it)
                true
            },
        )
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(middleware),
        )

        advanceUntilIdle()
        store.waitUntilIdle()
        store.dispatch(DefaultDesktopModeAction.ToggleDesktopMode).joinBlocking()
        advanceUntilIdle()
        store.waitUntilIdle()
    }

    @Test
    fun `GIVEN desktop mode is disabled WHEN the user toggles desktop mode on THEN the preference is updated`() = runTestOnMain {
        val expected = true
        val middleware = createMiddleware(
            scope = this,
            getDesktopBrowsingEnabled = { false },
            updateDesktopBrowsingEnabled = {
                assertEquals(expected, it)
                true
            },
        )
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(middleware),
        )

        advanceUntilIdle()
        store.waitUntilIdle()

        store.dispatch(DefaultDesktopModeAction.ToggleDesktopMode).joinBlocking()
    }

    @Test
    fun `GIVEN the user has toggled on desktop mode WHEN the preference update fails THEN the preference is reverted`() = runTestOnMain {
        val expected = false
        val middleware = createMiddleware(
            scope = this,
            getDesktopBrowsingEnabled = { expected },
            updateDesktopBrowsingEnabled = {
                false // trigger a failure
            },
        )
        val store = BrowserStore(
            initialState = BrowserState(
                desktopMode = expected,
            ),
            middleware = listOf(middleware),
        )

        advanceUntilIdle()
        store.waitUntilIdle()
        store.dispatch(DefaultDesktopModeAction.ToggleDesktopMode).joinBlocking()
        advanceUntilIdle()
        store.waitUntilIdle()

        launch {
            assertEquals(expected, store.state.desktopMode)
        }
    }

    @Test
    fun `GIVEN the user has toggled off desktop mode WHEN the preference update fails THEN the preference is reverted`() = runTestOnMain {
        val expected = true
        val middleware = createMiddleware(
            scope = this,
            getDesktopBrowsingEnabled = { expected },
            updateDesktopBrowsingEnabled = {
                false // trigger a failure
            },
        )
        val store = BrowserStore(
            initialState = BrowserState(
                desktopMode = expected,
            ),
            middleware = listOf(middleware),
        )

        advanceUntilIdle()
        store.waitUntilIdle()
        store.dispatch(DefaultDesktopModeAction.ToggleDesktopMode).joinBlocking()
        advanceUntilIdle()
        store.waitUntilIdle()

        launch {
            assertEquals(expected, store.state.desktopMode)
        }
    }

    private fun createMiddleware(
        scope: CoroutineScope,
        getDesktopBrowsingEnabled: () -> Boolean = { false },
        updateDesktopBrowsingEnabled: (Boolean) -> Boolean = { true },
    ) = DesktopModeMiddleware(
        scope = scope,
        repository = createRepository(
            getDesktopBrowsingEnabled = getDesktopBrowsingEnabled,
            updateDesktopBrowsingEnabled = updateDesktopBrowsingEnabled,
        ),
    )

    private fun createRepository(
        getDesktopBrowsingEnabled: () -> Boolean = { false },
        updateDesktopBrowsingEnabled: (Boolean) -> Boolean = { true },
    ) = object : DesktopModeRepository {
        override suspend fun getDesktopBrowsingEnabled(): Boolean {
            return getDesktopBrowsingEnabled()
        }

        override suspend fun setDesktopBrowsingEnabled(enabled: Boolean): Boolean {
            return updateDesktopBrowsingEnabled(enabled)
        }
    }
}
