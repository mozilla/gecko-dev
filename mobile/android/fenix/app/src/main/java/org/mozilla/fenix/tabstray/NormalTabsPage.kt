/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.fenix.tabstray.inactivetabs.InactiveTabsList

/**
 * UI for displaying the Normal Tabs Page in the Tabs Tray.
 *
 * @param normalTabs The list of active tabs to display.
 * @param inactiveTabs The list of inactive tabs to display.
 * @param selectedTabId The ID of the currently selected tab.
 * @param selectionMode [TabsTrayState.Mode] indicating whether the Tabs Tray is in single selection.
 * @param inactiveTabsExpanded Whether the Inactive Tabs section is expanded.
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
 * @param onTabDragStart Invoked when a tab drag has been started.
 */
@Composable
@Suppress("LongParameterList")
internal fun NormalTabsPage(
    normalTabs: List<TabSessionState>,
    inactiveTabs: List<TabSessionState>,
    selectedTabId: String?,
    selectionMode: TabsTrayState.Mode,
    inactiveTabsExpanded: Boolean,
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
    onTabDragStart: () -> Unit,
) {
    if (normalTabs.isNotEmpty() || inactiveTabs.isNotEmpty()) {
        val showInactiveTabsAutoCloseDialog by remember(inactiveTabs) {
            derivedStateOf {
                shouldShowInactiveTabsAutoCloseDialog(inactiveTabs.size)
            }
        }

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
            onTabDragStart = onTabDragStart,
            onMove = onMove,
        )
    } else {
        EmptyTabPage(isPrivate = false)
    }
}
