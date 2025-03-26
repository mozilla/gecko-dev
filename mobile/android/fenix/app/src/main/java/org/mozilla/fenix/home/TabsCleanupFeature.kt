/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import android.view.View
import kotlinx.coroutines.CoroutineScope
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.R
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
 * @param tabsUseCases The [TabsUseCases] instance to perform tab actions.
 * @param settings [Settings] used to check the application shared preferences.
 * @param snackBarParentView The [View] to find a parent from for displaying the snackbar.
 * @param showUndoSnackbar Lambda used to display an undo snackbar when a normal or private tab is
 * closed.
 * @param viewLifecycleScope The [CoroutineScope] to use for launching coroutines.
 */
@Suppress("LongParameterList")
class TabsCleanupFeature(
    private val context: Context,
    private val viewModel: HomeScreenViewModel,
    private val browserStore: BrowserStore,
    private val tabsUseCases: TabsUseCases,
    private val settings: Settings,
    private val snackBarParentView: View,
    private val showUndoSnackbar: (String) -> Unit,
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
            if (settings.feltPrivateBrowsingEnabled) {
                context.getString(R.string.snackbar_private_data_deleted)
            } else {
                context.getString(R.string.snackbar_private_tabs_closed)
            }
        } else {
            context.getString(R.string.snackbar_tabs_closed)
        }

        viewLifecycleScope.allowUndo(
            view = snackBarParentView,
            message = snackbarMessage,
            undoActionTitle = context.getString(R.string.snackbar_deleted_undo),
            onCancel = {
                tabsUseCases.undo.invoke()
            },
            operation = {},
        )
    }

    private fun removeTabAndShowSnackbar(sessionId: String) {
        val tab = browserStore.state.findTab(sessionId) ?: return
        tabsUseCases.removeTab(sessionId)
        showUndoSnackbar(context.tabClosedUndoMessage(tab.content.private))
    }
}
