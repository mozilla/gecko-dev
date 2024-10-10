/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.tabstrip

import android.graphics.Bitmap
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState

/**
 * The ui state of the tabs strip.
 *
 * @property tabs The list of [TabStripItem].
 * @property isPrivateMode Whether or not the browser is in private mode.
 */
data class TabStripState(
    val tabs: List<TabStripItem>,
    val isPrivateMode: Boolean,
) {
    companion object {
        val initial = TabStripState(tabs = emptyList(), isPrivateMode = false)
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
 */
data class TabStripItem(
    val id: String,
    val title: String,
    val url: String,
    val icon: Bitmap? = null,
    val isPrivate: Boolean,
    val isSelected: Boolean,
)

/**
 * Converts [BrowserState] to [TabStripState] that contains the information needed to render the
 * tabs strip. [TabStripState.isPrivateMode] is determined by the selected tab's privacy state when
 * [isSelectDisabled] is false. Otherwise, the private mode is determined by [isPossiblyPrivateMode].
 *
 * @param isSelectDisabled When true, the tabs will show as unselected.
 * @param isPossiblyPrivateMode Whether or not the browser is in private mode.
 */
internal fun BrowserState.toTabStripState(
    isSelectDisabled: Boolean,
    isPossiblyPrivateMode: Boolean,
): TabStripState {
    val isPrivateMode = if (isSelectDisabled) {
        isPossiblyPrivateMode
    } else {
        selectedTab?.content?.private == true
    }

    return TabStripState(
        tabs = getNormalOrPrivateTabs(private = isPrivateMode)
            .map {
                it.toTabStripItem(
                    isSelectDisabled = isSelectDisabled,
                    selectedTabId = selectedTabId,
                )
            },
        isPrivateMode = isPrivateMode,
    )
}

private fun TabSessionState.toTabStripItem(
    isSelectDisabled: Boolean,
    selectedTabId: String?,
) = TabStripItem(
    id = id,
    title = content.title.ifBlank { content.url },
    url = content.url,
    icon = content.icon,
    isPrivate = content.private,
    isSelected = !isSelectDisabled && id == selectedTabId,
)
