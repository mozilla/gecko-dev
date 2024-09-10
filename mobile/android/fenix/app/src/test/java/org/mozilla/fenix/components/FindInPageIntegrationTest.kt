/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.FrameLayout.LayoutParams.MATCH_PARENT
import androidx.core.view.isVisible
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.findinpage.view.FindInPageBar
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.appstate.AppAction.FindInPageAction

class FindInPageIntegrationTest {
    private val sessionId = "sessionId"
    private val engineViewBottomMargin = 11
    private val engineViewTopMargin = 22
    private val engineViewTranslationY = 10f
    private val findInPageHeight = 50
    private val store: BrowserStore = BrowserStore(
        initialState = BrowserState(
            tabs = listOf(createTab("https://www.mozilla.org", id = sessionId)),
            selectedTabId = sessionId,
        ),
    )
    private val appStore: AppStore = mockk(relaxed = true)
    private val findInPageBar: FindInPageBar = mockk(relaxed = true)
    private val engine: EngineView = mockk(relaxed = true)
    private val toolbar1: ViewGroup = mockk(relaxed = true)
    private val toolbar2: ViewGroup = mockk(relaxed = true)
    private val engineView: FrameLayout = mockk(relaxed = true)
    private val engineViewLayoutParams
        get() = engineView.layoutParams as FrameLayout.LayoutParams

    @Before
    fun setup() {
        configureInitialLayoutData()
    }

    @Test
    fun `WHEN find in page is requested in a normal tab THEN hide all toolbars, expand the engine view and show the find in page bar`() {
        val integration = buildFeature()

        integration.launch()

        assertEquals(true, integration.isFeatureActive)
        verify { engine.setDynamicToolbarMaxHeight(findInPageHeight) }
        verify { engineView.translationY = 0f }
        assertEquals(engineViewLayoutParams.topMargin, 0)
        assertEquals(engineViewLayoutParams.bottomMargin, findInPageHeight)
        listOf(toolbar1, toolbar2).forEach {
            verify { it.isVisible = false }
        }
        verify { findInPageBar.isVisible = true }
        verify { findInPageBar.layoutParams.height = findInPageHeight }
    }

    @Test
    fun `WHEN find in page is requested in a custom tab THEN hide all toolbars, expand the engine view and show the find in page bar`() {
        val customTabId = "customTabId"
        val store = BrowserStore(
            initialState = store.state.copy(
                customTabs = listOf(createCustomTab("https://www.mozilla.org", customTabId)),
            ),
        )
        val integration = buildFeature(store = store, customSessionId = customTabId)

        integration.launch()

        assertEquals(true, integration.isFeatureActive)
        verify { engine.setDynamicToolbarMaxHeight(findInPageHeight) }
        verify { engineView.translationY = 0f }
        assertEquals(engineViewLayoutParams.topMargin, 0)
        assertEquals(engineViewLayoutParams.bottomMargin, findInPageHeight)
        listOf(toolbar1, toolbar2).forEach {
            verify { it.isVisible = false }
        }
        verify { findInPageBar.isVisible = true }
        verify { findInPageBar.layoutParams.height = findInPageHeight }
    }

    @Test
    fun `GIVEN find in page is shown WHEN it is being dismissed THEN close it and restore previous layout`() {
        val integration = buildFeature()
        integration.launch()

        integration.feature.unbind()

        assertEquals(false, integration.isFeatureActive)
        verify { appStore.dispatch(FindInPageAction.FindInPageDismissed) }
        verify { findInPageBar.isVisible = false }
        val updatedContentHeight = toolbar1.height + toolbar2.height
        verify { engine.setDynamicToolbarMaxHeight(updatedContentHeight) }
        verify { engineView.translationY = engineViewTranslationY }
        assertEquals(engineViewLayoutParams.topMargin, engineViewTopMargin)
        assertEquals(engineViewLayoutParams.bottomMargin, engineViewBottomMargin)
        listOf(toolbar1, toolbar2).forEach {
            verify { it.isVisible = true }
        }
    }

    @Test
    fun `WHEN the app is no longer visible THEN dismiss the find in page bar`() {
        val integration = buildFeature()

        integration.stop()

        verify { appStore.dispatch(FindInPageAction.FindInPageDismissed) }
    }

    private fun configureInitialLayoutData(
        engineViewBottomMargin: Int = this.engineViewBottomMargin,
        engineViewTopMargin: Int = this.engineViewTopMargin,
        engineViewTranslationY: Float = this.engineViewTranslationY,
    ) {
        every { engine.asView().parent } returns engineView
        every { engineView.layoutParams } returns FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT).apply {
            topMargin = engineViewTopMargin
            bottomMargin = engineViewBottomMargin
        }
        every { engineView.translationY } returns engineViewTranslationY
        every { toolbar1.height } returns 33
        every { toolbar2.height } returns 44
    }

    private fun buildFeature(
        store: BrowserStore = this.store,
        toolbars: List<ViewGroup?> = listOf(toolbar1, null, toolbar2),
        customSessionId: String? = null,
    ): FindInPageIntegration {
        return FindInPageIntegration(
            store = store,
            appStore = appStore,
            sessionId = customSessionId,
            view = findInPageBar,
            engineView = engine,
            toolbars = toolbars,
            findInPageHeight = findInPageHeight,
        )
    }
}
