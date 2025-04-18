/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel

import androidx.core.net.toUri
import kotlinx.coroutines.Deferred
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.trustpanel.middleware.TrustPanelMiddleware
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelAction
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelState
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelStore
import org.mozilla.fenix.trackingprotection.TrackerBuckets

@RunWith(FenixRobolectricTestRunner::class)
class TrustPanelMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private lateinit var appStore: AppStore
    private lateinit var engine: Engine
    private lateinit var publicSuffixList: PublicSuffixList
    private lateinit var sessionUseCases: SessionUseCases
    private lateinit var trackingProtectionUseCases: TrackingProtectionUseCases

    @Before
    fun setup() {
        appStore = mock()
        engine = mock()
        publicSuffixList = mock()
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

    @Test
    fun `WHEN update trackers blocked action is dispatched THEN bucketed trackers state is updated`() = runTestOnMain {
        val url = "https://www.mozilla.org"
        val trackerLogList = listOf(
            TrackerLog(url = url, blockedCategories = listOf(TrackingCategory.FINGERPRINTING)),
        )
        val bucketedTrackers = spy(TrackerBuckets())

        val store = createStore(
            trustPanelState = TrustPanelState(bucketedTrackers = bucketedTrackers),
        )

        store.dispatch(TrustPanelAction.UpdateTrackersBlocked(trackerLogList))
        store.waitUntilIdle()

        verify(bucketedTrackers).updateIfNeeded(trackerLogList)
        assertEquals(store.state.numberOfTrackersBlocked, 1)
    }

    @Test
    fun `GIVEN the base domain is null WHEN request clear site data dialog action is dispatched THEN clear site data dialog is not launched`() = runTestOnMain {
        val url = "www.mozilla.org"
        val baseDomain = "mozilla.org"

        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.url).thenReturn(url)
        whenever(publicSuffixList.getPublicSuffixPlusOne(url)).thenReturn(null)

        val store = spy(
            createStore(
                trustPanelState = TrustPanelState(sessionState = sessionState),
            ),
        )

        store.dispatch(TrustPanelAction.RequestClearSiteDataDialog)
        store.waitUntilIdle()

        verify(store, never()).dispatch(TrustPanelAction.UpdateBaseDomain(baseDomain))
        verify(store, never()).dispatch(TrustPanelAction.Navigate.ClearSiteDataDialog)
    }

    @Test
    fun `GIVEN the base domain is not null WHEN request clear site data dialog action is dispatched THEN clear site data dialog is launched`() = runTestOnMain {
        val baseDomain = "mozilla.org"
        val url = "https://www.mozilla.org"
        val urlHost = url.toUri().host.orEmpty()

        val publicSuffixDeferredString: Deferred<String?> = mock()
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()

        whenever(sessionState.content).thenReturn(contentState)
        whenever(contentState.url).thenReturn(url)
        whenever(publicSuffixList.getPublicSuffixPlusOne(urlHost)).thenReturn(publicSuffixDeferredString)
        whenever(publicSuffixDeferredString.await()).thenReturn(baseDomain)

        val store = spy(
            createStore(
                trustPanelState = TrustPanelState(sessionState = sessionState),
            ),
        )

        store.dispatch(TrustPanelAction.RequestClearSiteDataDialog)
        store.waitUntilIdle()

        verify(store).dispatch(TrustPanelAction.UpdateBaseDomain(baseDomain))
        verify(store).dispatch(TrustPanelAction.Navigate.ClearSiteDataDialog)
    }

    @Test
    fun `WHEN clear site data action is dispatched THEN site data is cleared`() = runTestOnMain {
        val baseDomain = "mozilla.org"

        val store = createStore(
            trustPanelState = TrustPanelState(baseDomain = baseDomain),
        )

        store.dispatch(TrustPanelAction.ClearSiteData)
        store.waitUntilIdle()

        verify(engine).clearData(
            host = baseDomain,
            data = Engine.BrowsingData.select(
                Engine.BrowsingData.AUTH_SESSIONS,
                Engine.BrowsingData.ALL_SITE_DATA,
            ),
        )
        verify(appStore).dispatch(AppAction.SiteDataCleared)
    }

    private fun createStore(
        trustPanelState: TrustPanelState = TrustPanelState(),
        onDismiss: suspend () -> Unit = {},
    ) = TrustPanelStore(
        initialState = trustPanelState,
        middleware = listOf(
            TrustPanelMiddleware(
                appStore = appStore,
                engine = engine,
                publicSuffixList = publicSuffixList,
                sessionUseCases = sessionUseCases,
                trackingProtectionUseCases = trackingProtectionUseCases,
                onDismiss = onDismiss,
                scope = scope,
            ),
        ),
    )
}
