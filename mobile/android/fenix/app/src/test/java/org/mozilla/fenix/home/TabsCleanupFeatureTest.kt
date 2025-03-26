/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.View
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import kotlinx.coroutines.CoroutineScope
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.tabClosedUndoMessage
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.HomeScreenViewModel.Companion.ALL_NORMAL_TABS
import org.mozilla.fenix.home.HomeScreenViewModel.Companion.ALL_PRIVATE_TABS
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.utils.allowUndo

@RunWith(FenixRobolectricTestRunner::class)
class TabsCleanupFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val testCoroutineScope = coroutinesTestRule.scope

    @RelaxedMockK
    private lateinit var viewModel: HomeScreenViewModel

    @RelaxedMockK
    private lateinit var browserStore: BrowserStore

    @RelaxedMockK
    private lateinit var tabsUseCases: TabsUseCases

    @RelaxedMockK
    private lateinit var settings: Settings

    @RelaxedMockK
    private lateinit var snackBarParentView: View

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        mockkStatic("org.mozilla.fenix.utils.UndoKt")
        every {
            any<CoroutineScope>().allowUndo(
                view = any(),
                message = any(),
                undoActionTitle = any(),
                onCancel = any(),
                operation = any(),
                anchorView = any(),
                elevation = any(),
            )
        } just Runs
    }

    @Test
    fun `GIVEN all normal tabs to delete WHEN feature is started THEN remove all normal tabs and show undo snackbar`() {
        val feature = createTabsCleanupFeature()

        every { viewModel.sessionToDelete } returns ALL_NORMAL_TABS

        feature.start()

        verify {
            tabsUseCases.removeNormalTabs()

            testCoroutineScope.allowUndo(
                view = snackBarParentView,
                message = testContext.getString(R.string.snackbar_tabs_closed),
                undoActionTitle = testContext.getString(R.string.snackbar_deleted_undo),
                onCancel = any(),
                operation = any(),
            )

            viewModel.sessionToDelete = null
        }
    }

    @Test
    fun `GIVEN all private tabs to delete WHEN rfeature is started THEN remove all normal tabs and show undo snackbar`() {
        val feature = createTabsCleanupFeature()

        every { viewModel.sessionToDelete } returns ALL_PRIVATE_TABS

        feature.start()

        verify {
            tabsUseCases.removePrivateTabs()

            testCoroutineScope.allowUndo(
                view = snackBarParentView,
                message = testContext.getString(R.string.snackbar_private_tabs_closed),
                undoActionTitle = testContext.getString(R.string.snackbar_deleted_undo),
                onCancel = any(),
                operation = any(),
            )

            viewModel.sessionToDelete = null
        }
    }

    @Test
    fun `GIVEN all private tabs to delete and felt private browsing is enabled WHEN feature is started THEN remove all normal tabs and show undo snackbar`() {
        val feature = createTabsCleanupFeature()

        every { settings.feltPrivateBrowsingEnabled } returns true
        every { viewModel.sessionToDelete } returns ALL_PRIVATE_TABS

        feature.start()

        verify {
            tabsUseCases.removePrivateTabs()

            testCoroutineScope.allowUndo(
                view = snackBarParentView,
                message = testContext.getString(R.string.snackbar_private_data_deleted),
                undoActionTitle = testContext.getString(R.string.snackbar_deleted_undo),
                onCancel = any(),
                operation = any(),
            )

            viewModel.sessionToDelete = null
        }
    }

    @Test
    fun `GIVEN a session ID to delete WHEN feature is started THEN remove tab and show undo snackbar`() {
        var showUndoSnackbarCalled = false
        val showUndoSnackbar: (String) -> Unit = { _ ->
            showUndoSnackbarCalled = true
        }
        val feature = createTabsCleanupFeature(
            showUndoSnackbar = showUndoSnackbar,
        )
        val private = true
        val tab: TabSessionState = mockk { every { content.private } returns private }
        val sessionId = "1"

        mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        every { browserStore.state.findTab(sessionId) } returns tab
        every { viewModel.sessionToDelete } returns sessionId

        feature.start()

        assertTrue(showUndoSnackbarCalled)
        verify {
            tabsUseCases.removeTab(sessionId)
            showUndoSnackbar(testContext.tabClosedUndoMessage(private = private))
            viewModel.sessionToDelete = null
        }
    }

    private fun createTabsCleanupFeature(
        showUndoSnackbar: (String) -> Unit = {},
    ) = TabsCleanupFeature(
        context = testContext,
        viewModel = viewModel,
        browserStore = browserStore,
        settings = settings,
        tabsUseCases = tabsUseCases,
        snackBarParentView = snackBarParentView,
        showUndoSnackbar = showUndoSnackbar,
        viewLifecycleScope = testCoroutineScope,
    )
}
