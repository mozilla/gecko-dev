/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.store

import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.feature.addons.Addon
import mozilla.components.lib.state.State

/**
 * Value type that represents the state of the menu.
 *
 * @property browserMenuState The [BrowserMenuState] of the current browser session if any.
 * @property customTabSessionId The ID of the custom tab session if navigating from
 * an external access point, and null otherwise.
 * @property extensionMenuState The [ExtensionMenuState] to display.
 * @property isDesktopMode Whether or not the desktop mode is enabled for the currently visited
 * page.
 */
data class MenuState(
    val browserMenuState: BrowserMenuState? = null,
    val customTabSessionId: String? = null,
    val extensionMenuState: ExtensionMenuState = ExtensionMenuState(),
    val isDesktopMode: Boolean = false,
) : State

/**
 * Value type that represents the state of the browser menu.
 *
 * @property selectedTab The current selected [TabSessionState].
 * @property bookmarkState The [BookmarkState] of the selected tab.
 * @property isPinned Whether or not the selected tab is a pinned shortcut.
 */
data class BrowserMenuState(
    val selectedTab: TabSessionState,
    val bookmarkState: BookmarkState = BookmarkState(),
    val isPinned: Boolean = false,
)

/**
 * Value type that represents the state of the extension submenu.
 *
 * @property recommendedAddons A list of recommended [Addon]s to suggest.
 */
data class ExtensionMenuState(
    val recommendedAddons: List<Addon> = emptyList(),
)

/**
 * Value type that represents the bookmark state of a tab.
 *
 * @property guid The id of the bookmark.
 * @property isBookmarked Whether or not the selected tab is bookmarked.
 */
data class BookmarkState(
    val guid: String? = null,
    val isBookmarked: Boolean = false,
)
