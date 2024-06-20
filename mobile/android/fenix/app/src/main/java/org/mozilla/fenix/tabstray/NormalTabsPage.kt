/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.tabstray.inactivetabs.InactiveTabsList

/**
 * UI for displaying the Normal Tabs Page in the Tabs Tray.
 *
 * @param appStore [AppStore] used to listen for changes to [AppState].
 * @param tabsTrayStore [TabsTrayStore] used to listen for changes to [TabsTrayState].
 * @param displayTabsInGrid Whether the normal and private tabs should be displayed in a grid.
 * @param onTabClose Invoked when the user clicks to close a tab.
 * @param onTabMediaClick Invoked when the user interacts with a tab's media controls.
 * @param onTabClick Invoked when the user clicks on a tab.
 * @param onTabLongClick Invoked when the user long clicks a tab.
 * @param shouldShowInactiveTabsAutoCloseDialog Whether the inactive tabs auto close dialog should be displayed.
 * @param onInactiveTabsHeaderClick Invoked when the user clicks on the inactive tabs section header.
 * @param onDeleteAllInactiveTabsClick Invoked when the user clicks on the delete all inactive tabs button.
 * @param onInactiveTabsAutoCloseDialogShown Invoked when the inactive tabs auto close dialog
 * is presented to the user.
 * @param onInactiveTabAutoCloseDialogCloseButtonClick Invoked when the user clicks on the inactive
 * tab auto close dialog's dismiss button.
 * @param onEnableInactiveTabAutoCloseClick Invoked when the user clicks on the inactive tab auto
 * close dialog's enable button.
 * @param onInactiveTabClick Invoked when the user clicks on an inactive tab.
 * @param onInactiveTabClose Invoked when the user clicks on an inactive tab's close button.
 * @param onMove Invoked after the drag and drop gesture completed. Swaps position of two tabs.
 * @param shouldShowInactiveTabsCFR Returns whether the inactive tabs CFR is displayed.
 * @param onInactiveTabsCFRShown Invoked when the inactive tabs CFR is displayed.
 * @param onInactiveTabsCFRClick Invoked when the inactive tabs CFR is clicked.
 * @param onInactiveTabsCFRDismiss Invoked when the inactive tabs CFR is dismissed.
 */
@Composable
@Suppress("LongParameterList")
internal fun NormalTabsPage(
    appStore: AppStore,
    tabsTrayStore: TabsTrayStore,
    displayTabsInGrid: Boolean,
    onTabClose: (TabSessionState) -> Unit,
    onTabMediaClick: (TabSessionState) -> Unit,
    onTabClick: (TabSessionState) -> Unit,
    onTabLongClick: (TabSessionState) -> Unit,
    shouldShowInactiveTabsAutoCloseDialog: (Int) -> Boolean,
    onInactiveTabsHeaderClick: (Boolean) -> Unit,
    onDeleteAllInactiveTabsClick: () -> Unit,
    onInactiveTabsAutoCloseDialogShown: () -> Unit,
    onInactiveTabAutoCloseDialogCloseButtonClick: () -> Unit,
    onEnableInactiveTabAutoCloseClick: () -> Unit,
    onInactiveTabClick: (TabSessionState) -> Unit,
    onInactiveTabClose: (TabSessionState) -> Unit,
    onMove: (String, String?, Boolean) -> Unit,
    shouldShowInactiveTabsCFR: () -> Boolean,
    onInactiveTabsCFRShown: () -> Unit,
    onInactiveTabsCFRClick: () -> Unit,
    onInactiveTabsCFRDismiss: () -> Unit,
) {
    val inactiveTabsExpanded by appStore.observeAsState(
        initialValue = appStore.state.inactiveTabsExpanded,
    ) { state -> state.inactiveTabsExpanded }
    val selectedTabId by tabsTrayStore.observeAsState(
        initialValue = tabsTrayStore.state.selectedTabId,
    ) { state -> state.selectedTabId }
    val normalTabs by tabsTrayStore.observeAsState(
        initialValue = tabsTrayStore.state.normalTabs,
    ) { state -> state.normalTabs }
    val inactiveTabs by tabsTrayStore.observeAsState(
        initialValue = tabsTrayStore.state.inactiveTabs,
    ) { state -> state.inactiveTabs }
    val selectionMode by tabsTrayStore.observeAsState(
        initialValue = tabsTrayStore.state.mode,
    ) { state -> state.mode }

    if (normalTabs.isNotEmpty() || inactiveTabs.isNotEmpty()) {
        val showInactiveTabsAutoCloseDialog =
            shouldShowInactiveTabsAutoCloseDialog(inactiveTabs.size)
        var showAutoCloseDialog by remember { mutableStateOf(showInactiveTabsAutoCloseDialog) }

        val optionalInactiveTabsHeader: (@Composable () -> Unit)? = if (inactiveTabs.isEmpty()) {
            null
        } else {
            {
                InactiveTabsList(
                    inactiveTabs = inactiveTabs,
                    expanded = inactiveTabsExpanded,
                    showAutoCloseDialog = showAutoCloseDialog,
                    showCFR = shouldShowInactiveTabsCFR(),
                    onHeaderClick = onInactiveTabsHeaderClick,
                    onDeleteAllButtonClick = onDeleteAllInactiveTabsClick,
                    onAutoCloseDismissClick = {
                        onInactiveTabAutoCloseDialogCloseButtonClick()
                        showAutoCloseDialog = !showAutoCloseDialog
                    },
                    onEnableAutoCloseClick = {
                        onEnableInactiveTabAutoCloseClick()
                        showAutoCloseDialog = !showAutoCloseDialog
                    },
                    onTabClick = onInactiveTabClick,
                    onTabCloseClick = onInactiveTabClose,
                    onCFRShown = onInactiveTabsCFRShown,
                    onCFRClick = onInactiveTabsCFRClick,
                    onCFRDismiss = onInactiveTabsCFRDismiss,
                )
            }
        }

        if (showInactiveTabsAutoCloseDialog) {
            onInactiveTabsAutoCloseDialogShown()
        }

        TabLayout(
            tabs = normalTabs,
            displayTabsInGrid = displayTabsInGrid,
            selectedTabId = selectedTabId,
            selectionMode = selectionMode,
            modifier = Modifier.testTag(TabsTrayTestTag.normalTabsList),
            onTabClose = onTabClose,
            onTabMediaClick = onTabMediaClick,
            onTabClick = onTabClick,
            onTabLongClick = onTabLongClick,
            header = optionalInactiveTabsHeader,
            onTabDragStart = { tabsTrayStore.dispatch(TabsTrayAction.ExitSelectMode) },
            onMove = onMove,
        )
    } else {
        EmptyTabPage(isPrivate = false)
    }
}
