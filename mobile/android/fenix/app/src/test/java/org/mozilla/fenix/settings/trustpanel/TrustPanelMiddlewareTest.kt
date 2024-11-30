/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel

import mozilla.components.browser.state.state.SessionState
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.trustpanel.middleware.TrustPanelMiddleware
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelAction
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelState
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelStore

@RunWith(FenixRobolectricTestRunner::class)
class TrustPanelMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private lateinit var sessionUseCases: SessionUseCases
    private lateinit var trackingProtectionUseCases: TrackingProtectionUseCases

    @Before
    fun setup() {
        sessionUseCases = mock()
        trackingProtectionUseCases = mock()
    }

    @Test
    fun `GIVEN tracking protection is enabled WHEN toggle tracking protection action is dispatched THEN tracking protection exception is added`() = runTestOnMain {
        val sessionId = "0"
        val sessionState: SessionState = mock()
        val reloadUrlUseCase: SessionUseCases.ReloadUrlUseCase = mock()
        val addExceptionUseCase: TrackingProtectionUseCases.AddExceptionUseCase = mock()

        whenever(sessionState.id).thenReturn(sessionId)
        whenever(sessionUseCases.reload).thenReturn(reloadUrlUseCase)
        whenever(trackingProtectionUseCases.addException).thenReturn(addExceptionUseCase)

        val store = createStore(
            trustPanelState = TrustPanelState(
                isTrackingProtectionEnabled = true,
                sessionState = sessionState,
            ),
        )

        store.dispatch(TrustPanelAction.ToggleTrackingProtection)
        store.waitUntilIdle()

        verify(addExceptionUseCase).invoke(sessionId)
        verify(reloadUrlUseCase).invoke(sessionId)
    }

    @Test
    fun `GIVEN tracking protection is disabled WHEN toggle tracking protection action is dispatched THEN tracking protection exception is removed`() = runTestOnMain {
        val sessionId = "0"
        val sessionState: SessionState = mock()
        val reloadUrlUseCase: SessionUseCases.ReloadUrlUseCase = mock()
        val removeExceptionUseCase: TrackingProtectionUseCases.RemoveExceptionUseCase = mock()

        whenever(sessionState.id).thenReturn(sessionId)
        whenever(sessionUseCases.reload).thenReturn(reloadUrlUseCase)
        whenever(trackingProtectionUseCases.removeException).thenReturn(removeExceptionUseCase)

        val store = createStore(
            trustPanelState = TrustPanelState(
                isTrackingProtectionEnabled = false,
                sessionState = sessionState,
            ),
        )

        store.dispatch(TrustPanelAction.ToggleTrackingProtection)
        store.waitUntilIdle()

        verify(removeExceptionUseCase).invoke(sessionId)
        verify(reloadUrlUseCase).invoke(sessionId)
    }

    private fun createStore(
        trustPanelState: TrustPanelState = TrustPanelState(),
    ) = TrustPanelStore(
        initialState = trustPanelState,
        middleware = listOf(
            TrustPanelMiddleware(
                sessionUseCases = sessionUseCases,
                trackingProtectionUseCases = trackingProtectionUseCases,
                scope = scope,
            ),
        ),
    )
}
