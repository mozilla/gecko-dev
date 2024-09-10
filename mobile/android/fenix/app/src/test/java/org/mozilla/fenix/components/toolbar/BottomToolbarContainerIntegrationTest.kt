/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import androidx.core.view.isVisible
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.FindInPageIntegration
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class BottomToolbarContainerIntegrationTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var feature: BottomToolbarContainerIntegration
    private lateinit var appStore: AppStore
    private lateinit var toolbarContainer: ToolbarContainerView
    private val findInPageIntegration: FindInPageIntegration = mockk()

    @Before
    fun setup() {
        appStore = AppStore()
        toolbarContainer = spyk(ToolbarContainerView(testContext))
        every { findInPageIntegration.isFeatureActive } returns false

        val bottomToolbarContainerView: BottomToolbarContainerView = mockk()
        feature = BottomToolbarContainerIntegration(
            toolbar = mockk(),
            store = mockk(),
            appStore = appStore,
            bottomToolbarContainerView = bottomToolbarContainerView,
            sessionId = null,
            findInPageFeature = { findInPageIntegration },
        ).apply {
            toolbarController = mockk(relaxed = true)
        }

        every { testContext.components.settings } returns mockk(relaxed = true)
        every { toolbarContainer.context } returns testContext
        every { bottomToolbarContainerView.toolbarContainerView } returns toolbarContainer
        every { testContext.components.settings.toolbarPosition } returns ToolbarPosition.BOTTOM
    }

    @Test
    fun `WHEN the feature starts THEN toolbar controllers starts as well`() {
        feature.start()

        verify { feature.toolbarController.start() }
    }

    @Test
    fun `WHEN the feature stops THEN toolbar controllers stops as well`() {
        feature.stop()

        verify { feature.toolbarController.stop() }
    }

    @Test
    fun `GIVEN toolbar at bottom and find in page not active WHEN search dialog becomes visible THEN toolbar gets hidden`() {
        val searchDialogVisibility = true
        val toolbarContainerVisibility = false

        feature.start()
        appStore.dispatch(AppAction.UpdateSearchDialogVisibility(isVisible = searchDialogVisibility)).joinBlocking()

        assertEquals(toolbarContainerVisibility, toolbarContainer.isVisible)
    }

    @Test
    fun `GIVEN toolbar at bottom and find in page not active WHEN search dialog is not visible THEN toolbar is visible`() {
        val searchDialogVisibility = false
        val toolbarContainerVisibility = true

        feature.start()
        appStore.dispatch(AppAction.UpdateSearchDialogVisibility(isVisible = searchDialogVisibility)).joinBlocking()

        assertEquals(toolbarContainerVisibility, toolbarContainer.isVisible)
    }

    @Test
    fun `GIVEN toolbar at bottom and find in page active WHEN search dialog is not visible THEN toolbar visibility does not change`() {
        val searchDialogVisibility = false
        every { findInPageIntegration.isFeatureActive } returns true

        feature.start()
        appStore.dispatch(AppAction.UpdateSearchDialogVisibility(isVisible = searchDialogVisibility)).joinBlocking()

        verify(exactly = 0) { toolbarContainer.visibility = any() }
    }

    @Test
    fun `GIVEN toolbar positioned at top and find in page not active WHEN search dialog visibility changes THEN toolbar visibility remains unchanged`() {
        every { testContext.components.settings.toolbarPosition } returns ToolbarPosition.TOP
        var searchDialogVisibility = false
        val toolbarContainerVisibility = true

        feature.start()
        appStore.dispatch(AppAction.UpdateSearchDialogVisibility(isVisible = searchDialogVisibility)).joinBlocking()

        assertEquals(toolbarContainerVisibility, toolbarContainer.isVisible)

        searchDialogVisibility = true
        appStore.dispatch(AppAction.UpdateSearchDialogVisibility(isVisible = searchDialogVisibility)).joinBlocking()

        assertEquals(toolbarContainerVisibility, toolbarContainer.isVisible)
    }

    @Test
    fun `GIVEN toolbar positioned at top and find in page active WHEN search dialog visibility changes THEN toolbar visibility remains unchanged`() {
        every { testContext.components.settings.toolbarPosition } returns ToolbarPosition.TOP
        var searchDialogVisibility = false
        every { findInPageIntegration.isFeatureActive } returns true

        feature.start()
        appStore.dispatch(AppAction.UpdateSearchDialogVisibility(isVisible = searchDialogVisibility)).joinBlocking()

        verify(exactly = 0) { toolbarContainer.visibility = any() }

        searchDialogVisibility = true
        appStore.dispatch(AppAction.UpdateSearchDialogVisibility(isVisible = searchDialogVisibility)).joinBlocking()

        verify(exactly = 0) { toolbarContainer.visibility = any() }
    }
}
