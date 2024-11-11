/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.tabstrip

import android.graphics.Bitmap
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState

private const val MAX_TABS_WITH_CLOSE_BUTTON_VISIBLE = 7

/**
 * The ui state of the tabs strip.
 *
 * @property tabs The list of [TabStripItem].
 * @property isPrivateMode Whether or not the browser is in private mode.
 * @property tabCounterMenuItems The list of [TabCounterMenuItem]s to be displayed in the tab
 * counter menu.
 */
data class TabStripState(
    val tabs: List<TabStripItem>,
    val isPrivateMode: Boolean,
    val tabCounterMenuItems: List<TabCounterMenuItem>,
) {

    val menuItems
        get() = tabCounterMenuItems.map { it.toMenuItem() }

    companion object {
        val initial = TabStripState(
            tabs = emptyList(),
            isPrivateMode = false,
            tabCounterMenuItems = emptyList(),
        )
    }
}

/**
 * The ui state of a tab.
 *
 * @property id The id of the tab.
 * @property title The title of the tab.
 * @property url The url of the tab.
 * @property icon The icon of the tab.
 * @property isPrivate Whether or not the tab is private.
 * @property isSelected Whether or not the tab is selected.
 * @property isCloseButtonVisible Whether or not the close button is visible.
 */
data class TabStripItem(
    val id: String,
    val title: String,
    val url: String,
    val icon: Bitmap? = null,
    val isPrivate: Boolean,
    val isSelected: Boolean,
    val isCloseButtonVisible: Boolean = true,
)

/**
 * Converts [BrowserState] to [TabStripState] that contains the information needed to render the
 * tabs strip. [TabStripState.isPrivateMode] is determined by the selected tab's privacy state when
 * [isSelectDisabled] is false. Otherwise, the private mode is determined by [isPossiblyPrivateMode].
 *
 * @param isSelectDisabled When true, the tabs will show as unselected.
 * @param isPossiblyPrivateMode Whether or not the browser is in private mode.
 * @param addTab Invoked when conditions are met for adding a new normal browsing mode tab.
 * @param toggleBrowsingMode Invoked when conditions are met for toggling the browsing mode.
 * @param closeTab Invoked when close tab is clicked.
 */
internal fun BrowserState.toTabStripState(
    isSelectDisabled: Boolean,
    isPossiblyPrivateMode: Boolean,
    addTab: () -> Unit,
    toggleBrowsingMode: (isCurrentlyPrivate: Boolean) -> Unit,
    closeTab: (isPrivate: Boolean, numberOfTabs: Int) -> Unit,
): TabStripState {
    val isPrivateMode = if (isSelectDisabled) {
        isPossiblyPrivateMode
    } else {
        selectedTab?.content?.private == true
    }

    val tabs = getNormalOrPrivateTabs(private = isPrivateMode)

    return TabStripState(
        tabs = tabs
            .map {
                it.toTabStripItem(
                    isSelectDisabled = isSelectDisabled,
                    selectedTabId = selectedTabId,
                    showCloseButtonOnUnselectedTabs = tabs.size <= MAX_TABS_WITH_CLOSE_BUTTON_VISIBLE,
                )
            },
        isPrivateMode = isPrivateMode,
        tabCounterMenuItems = mapToMenuItems(
            isSelectEnabled = !isSelectDisabled,
            isPrivateMode = isPrivateMode,
            addTab = addTab,
            toggleBrowsingMode = toggleBrowsingMode,
            closeTab = closeTab,
            numberOfTabs = tabs.size,
        ),
    )
}

private fun mapToMenuItems(
    isSelectEnabled: Boolean,
    isPrivateMode: Boolean,
    toggleBrowsingMode: (isCurrentlyPrivate: Boolean) -> Unit,
    addTab: () -> Unit,
    closeTab: (isPrivate: Boolean, numberOfTabs: Int) -> Unit,
    numberOfTabs: Int,
): List<TabCounterMenuItem> = buildList {
    if (isSelectEnabled || isPrivateMode) {
        val onClick = {
            if (isPrivateMode) {
                toggleBrowsingMode(true)
            } else {
                addTab()
            }
        }
        add(TabCounterMenuItem.IconItem.NewTab(onClick = onClick))
    }

    if (isSelectEnabled || !isPrivateMode) {
        val onClick = {
            if (isPrivateMode) {
                addTab()
            } else {
                toggleBrowsingMode(false)
            }
        }
        add(TabCounterMenuItem.IconItem.NewPrivateTab(onClick = onClick))
    }

    if (isSelectEnabled) {
        add(TabCounterMenuItem.Divider)
        add(
            TabCounterMenuItem.IconItem.CloseTab(
                onClick = { closeTab(isPrivateMode, numberOfTabs) },
            ),
        )
    }
}

private fun TabSessionState.toTabStripItem(
    isSelectDisabled: Boolean,
    selectedTabId: String?,
    showCloseButtonOnUnselectedTabs: Boolean,
): TabStripItem {
    val isSelected = !isSelectDisabled && id == selectedTabId
    return TabStripItem(
        id = id,
        title = content.title.ifBlank { content.url },
        url = content.url,
        icon = content.icon,
        isPrivate = content.private,
        isSelected = isSelected,
        isCloseButtonVisible = showCloseButtonOnUnselectedTabs || isSelected,
    )
}
