/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.View
import androidx.navigation.NavController
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.CoroutineScope
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.usecases.FenixBrowserUseCases
import org.mozilla.fenix.home.HomeScreenViewModel.Companion.ALL_NORMAL_TABS
import org.mozilla.fenix.home.HomeScreenViewModel.Companion.ALL_PRIVATE_TABS
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.utils.allowUndo
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class TabsCleanupFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val testCoroutineScope = coroutinesTestRule.scope

    @RelaxedMockK
    private lateinit var viewModel: HomeScreenViewModel

    @RelaxedMockK
    private lateinit var browserStore: BrowserStore

    @RelaxedMockK
    private lateinit var browsingModeManager: BrowsingModeManager

    @RelaxedMockK
    private lateinit var navController: NavController

    @RelaxedMockK
    private lateinit var tabsUseCases: TabsUseCases

    @RelaxedMockK
    private lateinit var fenixBrowserUseCases: FenixBrowserUseCases

    @RelaxedMockK
    private lateinit var settings: Settings

    @RelaxedMockK
    private lateinit var snackBarParentView: View

    private lateinit var feature: TabsCleanupFeature

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

        feature = TabsCleanupFeature(
            context = testContext,
            viewModel = viewModel,
            browserStore = browserStore,
            browsingModeManager = browsingModeManager,
            navController = navController,
            settings = settings,
            tabsUseCases = tabsUseCases,
            fenixBrowserUseCases = fenixBrowserUseCases,
            snackBarParentView = snackBarParentView,
            viewLifecycleScope = testCoroutineScope,
        )
    }

    @Test
    fun `GIVEN all normal tabs to delete WHEN feature is started THEN remove all normal tabs and show undo snackbar`() {
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
    fun `GIVEN homepage as a new tab is enabled and all normal tabs to delete WHEN feature is started THEN remove all normal tabs and show undo snackbar`() {
        every { settings.enableHomepageAsNewTab } returns true
        every { viewModel.sessionToDelete } returns ALL_NORMAL_TABS

        feature.start()

        verifyOrder {
            tabsUseCases.removeNormalTabs()

            fenixBrowserUseCases.addNewHomepageTab(
                private = browsingModeManager.mode.isPrivate,
            )

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
    fun `GIVEN homepage as a new tab is enabled and all private tabs to delete WHEN rfeature is started THEN remove all normal tabs, show undo snackbar and ensure 1 new tab remains`() {
        every { settings.enableHomepageAsNewTab } returns true
        every { viewModel.sessionToDelete } returns ALL_PRIVATE_TABS

        feature.start()

        verifyOrder {
            tabsUseCases.removePrivateTabs()

            fenixBrowserUseCases.addNewHomepageTab(
                private = browsingModeManager.mode.isPrivate,
            )

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
    fun `GIVEN felt private browsing is enabled and all private tabs to delete WHEN remove tabs is called THEN remove all normal tabs and show undo snackbar`() {
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
        val private = true
        val tab: TabSessionState = mockk { every { content.private } returns private }
        val sessionId = "1"

        mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        every { browserStore.state.findTab(sessionId) } returns tab
        every { viewModel.sessionToDelete } returns sessionId

        feature.start()

        verify {
            tabsUseCases.removeTab(sessionId)

            testCoroutineScope.allowUndo(
                view = snackBarParentView,
                message = testContext.getString(R.string.snackbar_private_tab_closed),
                undoActionTitle = testContext.getString(R.string.snackbar_deleted_undo),
                onCancel = any(),
                operation = any(),
            )

            viewModel.sessionToDelete = null
        }
    }

    @Test
    fun `GIVEN homepage as a new tab is enabled and the last tab is to be removed WHEN feature is started THEN remove tab, show undo snackbar and ensure a new tab remains`() {
        val private = true
        val tab: TabSessionState = mockk { every { content.private } returns private }
        val sessionId = "1"

        every { settings.enableHomepageAsNewTab } returns true
        every { browsingModeManager.mode.isPrivate } returns private
        every { viewModel.sessionToDelete } returns sessionId

        mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        every { browserStore.state.findTab(sessionId) } returns tab
        every { browserStore.state.privateTabs.size } returns 1

        feature.start()

        verifyOrder {
            tabsUseCases.removeTab(sessionId)

            fenixBrowserUseCases.addNewHomepageTab(
                private = private,
            )

            testCoroutineScope.allowUndo(
                view = snackBarParentView,
                message = testContext.getString(R.string.snackbar_private_tab_closed),
                undoActionTitle = testContext.getString(R.string.snackbar_deleted_undo),
                onCancel = any(),
                operation = any(),
            )

            viewModel.sessionToDelete = null
        }
    }

    @Test
    fun `GIVEN homepage as a new tab is enabled and a session ID to delete WHEN feature is started THEN remove tab and show undo snackbar`() {
        val private = true
        val tab: TabSessionState = mockk { every { content.private } returns private }
        val sessionId = "1"

        every { settings.enableHomepageAsNewTab } returns true
        every { browsingModeManager.mode.isPrivate } returns private
        every { viewModel.sessionToDelete } returns sessionId

        mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        every { browserStore.state.findTab(sessionId) } returns tab
        every { browserStore.state.privateTabs.size } returns 2

        feature.start()

        verify {
            tabsUseCases.removeTab(sessionId)

            testCoroutineScope.allowUndo(
                view = snackBarParentView,
                message = testContext.getString(R.string.snackbar_private_tab_closed),
                undoActionTitle = testContext.getString(R.string.snackbar_deleted_undo),
                onCancel = any(),
                operation = any(),
            )

            viewModel.sessionToDelete = null
        }

        verify(exactly = 0) {
            fenixBrowserUseCases.addNewHomepageTab(
                private = private,
            )
        }
    }

    @Test
    fun `WHEN undo all tabs removed is called THEN undo tab removal`() {
        feature.onUndoAllTabsRemoved(tabId = "")

        verify {
            tabsUseCases.undo.invoke()
        }
    }

    @Test
    fun `GIVEN a tab ID WHEN undo all tabs removed is called THEN undo tab removal and remove the tab`() {
        val tabId = "1"

        feature.onUndoAllTabsRemoved(tabId = tabId)

        verifyOrder {
            tabsUseCases.undo.invoke()
            tabsUseCases.removeTab.invoke(tabId)
        }
    }

    @Test
    fun `WHEN undo tab removed is called THEN undo tab removal and navigate to browser`() {
        feature.onUndoTabRemoved(tabId = "")

        verify {
            tabsUseCases.undo.invoke()

            navController.navigate(
                HomeFragmentDirections.actionGlobalBrowser(null),
            )
        }
    }

    @Test
    fun `GIVEN a tab ID WHEN undo tab removed is called THEN undo tab removal, remove the tab and navigate to browser`() {
        val tabId = "1"

        feature.onUndoTabRemoved(tabId = tabId)

        verifyOrder {
            tabsUseCases.undo.invoke()
            tabsUseCases.removeTab.invoke(tabId)
            navController.navigate(
                HomeFragmentDirections.actionGlobalBrowser(null),
            )
        }
    }
}
