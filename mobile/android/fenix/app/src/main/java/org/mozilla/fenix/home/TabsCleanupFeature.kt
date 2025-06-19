/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.usecases.FenixBrowserUseCases
import org.mozilla.fenix.ext.tabClosedUndoMessage
import org.mozilla.fenix.home.HomeScreenViewModel.Companion.ALL_NORMAL_TABS
import org.mozilla.fenix.home.HomeScreenViewModel.Companion.ALL_PRIVATE_TABS
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.utils.allowUndo

/**
 * Delegate to handle tab removal and undo actions in the homepage.
 *
 * @param context An Android [Context].
 * @param viewModel [HomeScreenViewModel] containing the data on the sessions to delete.
 * @param browserStore The [BrowserStore] that holds the currently open tabs.
 * @param browsingModeManager [BrowsingModeManager] used for fetching the current browsing mode.
 * @param navController [NavController] used for navigation.
 * @param tabsUseCases The [TabsUseCases] instance to perform tab actions.
 * @param fenixBrowserUseCases [FenixBrowserUseCases] used for adding new homepage tabs.
 * @param settings [Settings] used to check the application shared preferences.
 * @param snackBarParentView The [View] to find a parent from for displaying the snackbar.
 * @param viewLifecycleScope The [CoroutineScope] to use for launching coroutines.
 */
@Suppress("LongParameterList")
class TabsCleanupFeature(
    private val context: Context,
    private val viewModel: HomeScreenViewModel,
    private val browserStore: BrowserStore,
    private val browsingModeManager: BrowsingModeManager,
    private val navController: NavController,
    private val tabsUseCases: TabsUseCases,
    private val fenixBrowserUseCases: FenixBrowserUseCases,
    private val settings: Settings,
    private val snackBarParentView: View,
    private val viewLifecycleScope: CoroutineScope,
) : LifecycleAwareFeature {

    /**
     * Removes the sessions that have been queued for deletion when the home screen is started.
     */
    override fun start() {
        viewModel.sessionToDelete?.also {
            if (it == ALL_NORMAL_TABS || it == ALL_PRIVATE_TABS) {
                removeAllTabsAndShowSnackbar(it)
            } else {
                removeTabAndShowSnackbar(it)
            }
        }

        viewModel.sessionToDelete = null
    }

    override fun stop() = Unit

    private fun removeAllTabsAndShowSnackbar(sessionCode: String) {
        if (sessionCode == ALL_PRIVATE_TABS) {
            tabsUseCases.removePrivateTabs()
        } else {
            tabsUseCases.removeNormalTabs()
        }

        val snackbarMessage = if (sessionCode == ALL_PRIVATE_TABS) {
            context.getString(R.string.snackbar_private_data_deleted)
        } else {
            context.getString(R.string.snackbar_tabs_closed)
        }

        var tabId: String? = null
        if (settings.enableHomepageAsNewTab) {
            // Add a new tab after all the tabs are removed to ensure there's always 1 tab.
            // Hold onto the new tab ID so that the new tab can be removed if the tabs are restored
            // by the undo action.
            tabId = fenixBrowserUseCases.addNewHomepageTab(
                private = browsingModeManager.mode.isPrivate,
            )
        }

        viewLifecycleScope.allowUndo(
            view = snackBarParentView,
            message = snackbarMessage,
            undoActionTitle = context.getString(R.string.snackbar_deleted_undo),
            onCancel = {
                onUndoAllTabsRemoved(tabId)
            },
            operation = {},
        )
    }

    /**
     * Callback invoked when the remove all tabs action is cancelled.
     *
     * @param tabId Optional ID of the tab that should be removed after the tab removal is
     * undone.
     */
    @VisibleForTesting
    internal fun onUndoAllTabsRemoved(tabId: String?) {
        tabsUseCases.undo.invoke()

        if (tabId?.isNotBlank() == true) {
            tabsUseCases.removeTab.invoke(tabId)
        }
    }

    private fun removeTabAndShowSnackbar(sessionId: String) {
        val tab = browserStore.state.findTab(sessionId) ?: return
        val isPrivate = browsingModeManager.mode.isPrivate

        // Check if this is the last tab being removed.
        val hasTabsRemaining = if (isPrivate) {
            browserStore.state.privateTabs.size > 1
        } else {
            browserStore.state.normalTabs.size > 1
        }

        tabsUseCases.removeTab(sessionId)

        var tabId = ""
        if (settings.enableHomepageAsNewTab && !hasTabsRemaining) {
            // Add a new tab if the last tab is being removed to ensure there's always 1 tab.
            // Hold onto the new tab ID so that the new tab can be removed if the tabs are restored
            // by the undo action.
            tabId = fenixBrowserUseCases.addNewHomepageTab(
                private = isPrivate,
            )
        }

        viewLifecycleScope.allowUndo(
            view = snackBarParentView,
            message = context.tabClosedUndoMessage(tab.content.private),
            undoActionTitle = context.getString(R.string.snackbar_deleted_undo),
            onCancel = {
                onUndoTabRemoved(tabId)
            },
            operation = {},
        )
    }

    /**
     * Callback invoked when the remove tab action is cancelled.
     *
     * @param tabId Optional ID of the tab that should be removed after the tab removal is
     * undone.
     */
    @VisibleForTesting
    internal fun onUndoTabRemoved(tabId: String?) {
        tabsUseCases.undo.invoke()

        if (tabId?.isNotBlank() == true) {
            tabsUseCases.removeTab.invoke(tabId)
        }

        navController.navigate(
            HomeFragmentDirections.actionGlobalBrowser(null),
        )
    }
}
