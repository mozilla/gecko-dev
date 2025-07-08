/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.webcompat.WebCompatState
import org.mozilla.fenix.webcompat.store.WebCompatReporterAction
import org.mozilla.fenix.webcompat.store.WebCompatReporterState
import org.mozilla.fenix.webcompat.store.WebCompatReporterStore

@RunWith(AndroidJUnit4::class)
class WebCompatReporterStorageMiddlewareTest {

    @Test
    fun `GIVEN a previous session with the same URL WHEN the store is initialized THEN the previous state is restored`() {
        val matchingURL = "www.mozilla.org"
        val expected = WebCompatState(
            tabUrl = matchingURL,
            enteredUrl = matchingURL,
            reason = WebCompatReporterState.BrokenSiteReason.Media.name,
            problemDescription = "A user provided problem description.",
        )
        val initialState = WebCompatReporterState(
            tabUrl = matchingURL,
            enteredUrl = matchingURL,
        )
        val store = createWebCompatReporterStore(
            initialState = initialState,
            appStore = createAppStore(
                webCompatState = expected,
            ),
        )

        assertEquals(expected.toReporterState(), store.state)
        assertNotEquals(initialState, store.state)
    }

    @Test
    fun `GIVEN a previous session with a different URL WHEN the store is initialized THEN the previous state is not restored`() {
        val previousState = WebCompatState(
            tabUrl = "www.mozilla.org",
            enteredUrl = "www.mozilla.org",
            reason = WebCompatReporterState.BrokenSiteReason.Media.name,
            problemDescription = "A user provided problem description.",
        )
        val expected = WebCompatReporterState(
            tabUrl = "www.example.com",
            enteredUrl = "www.example.com",
        )
        val store = createWebCompatReporterStore(
            initialState = expected,
            appStore = createAppStore(
                webCompatState = previousState,
            ),
        )

        assertNotEquals(previousState.toReporterState(), store.state)
        assertEquals(expected, store.state)
    }

    @Test
    fun `GIVEN the URL text field is empty WHEN the user leaves the form THEN the tab's URL is saved instead`() {
        val expectedUrl = "www.mozilla.org"
        val appStore = AppStore()
        val webCompatReporterStore = createWebCompatReporterStore(
            initialState = WebCompatReporterState(
                tabUrl = expectedUrl,
                enteredUrl = "",
            ),
            appStore = appStore,
        )

        webCompatReporterStore.dispatch(WebCompatReporterAction.BackPressed)
        appStore.waitUntilIdle()

        assertEquals(expectedUrl, appStore.state.webCompatState!!.tabUrl)
        assertTrue(appStore.state.webCompatState!!.tabUrl.isNotEmpty())
    }

    @Test
    fun `WHEN the back button is pressed THEN the state is saved`() {
        val savedState = WebCompatReporterState(
            enteredUrl = "www.mozilla.org",
            reason = null,
            problemDescription = "problem description",
        )
        val appStore = AppStore()
        val webCompatReporterStore = createWebCompatReporterStore(
            initialState = savedState,
            appStore = appStore,
        )

        webCompatReporterStore.dispatch(WebCompatReporterAction.BackPressed)
        appStore.waitUntilIdle()

        assertEquals(savedState.toPersistedState(), appStore.state.webCompatState)
    }

    @Test
    fun `WHEN the send more info button is pressed THEN the state is saved`() {
        val savedState = WebCompatReporterState(
            enteredUrl = "www.mozilla.org",
            reason = null,
            problemDescription = "problem description",
        )
        val appStore = AppStore()
        val webCompatReporterStore = createWebCompatReporterStore(
            initialState = savedState,
            appStore = appStore,
        )

        webCompatReporterStore.dispatch(WebCompatReporterAction.BackPressed)
        appStore.waitUntilIdle()

        assertEquals(savedState.toPersistedState(), appStore.state.webCompatState)
    }

    @Test
    fun `WHEN the cancel button is pressed THEN the state is cleared`() {
        val appStore = createAppStore(
            webCompatState = WebCompatState(
                tabUrl = "www.mozilla.org",
                reason = null,
                problemDescription = "problem description",
            ),
        )
        val webCompatReporterStore = createWebCompatReporterStore(
            appStore = appStore,
        )

        webCompatReporterStore.dispatch(WebCompatReporterAction.CancelClicked)
        appStore.waitUntilIdle()

        assertNull(appStore.state.webCompatState)
    }

    private fun createWebCompatReporterStore(
        initialState: WebCompatReporterState = WebCompatReporterState(),
        appStore: AppStore = createAppStore(),
    ) = WebCompatReporterStore(
        initialState = initialState,
        middleware = listOf(
            WebCompatReporterStorageMiddleware(
                appStore = appStore,
            ),
        ),
    )

    private fun createAppStore(
        webCompatState: WebCompatState? = null,
    ) = AppStore(
        initialState = AppState(
            webCompatState = webCompatState,
        ),
    )
}
