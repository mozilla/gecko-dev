/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.store

import android.graphics.Bitmap
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
 * @property availableAddons A list of installed and enabled [Addon]s to be shown.
 * @property showExtensionsOnboarding Show extensions promotion banner onboarding.
 * @property showDisabledExtensionsOnboarding Show extensions promotion banner onboarding when
 * all installed extensions have been disabled.
 * @property addonInstallationInProgress The [Addon] that is currently being installed.
 * @property shouldShowManageExtensionsMenuItem Indicates if manage extensions menu item
 * should be displayed to the user.
 * @property browserWebExtensionMenuItem A list of [WebExtensionMenuItem]s
 * to be shown in the menu.
 */
data class ExtensionMenuState(
    val recommendedAddons: List<Addon> = emptyList(),
    val availableAddons: List<Addon> = emptyList(),
    val showExtensionsOnboarding: Boolean = false,
    val showDisabledExtensionsOnboarding: Boolean = false,
    val addonInstallationInProgress: Addon? = null,
    val shouldShowManageExtensionsMenuItem: Boolean = false,
    val browserWebExtensionMenuItem: List<WebExtensionMenuItem> = emptyList(),
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

/**
 * Installed extensions actions to display relevant to the browser as a whole.
 *
 * @property label The label of the web extension menu item.
 * @property enabled Indicates if web extension menu item should be enabled or disabled.
 * @property icon The icon that should be shown in the menu.
 * @property badgeText Menu item badge text.
 * @property badgeTextColor Menu item badge text color.
 * @property badgeBackgroundColor Menu item badge background color.
 * @property onClick A callback to be executed when the web extension menu item is clicked.
 */
data class WebExtensionMenuItem(
    val label: String,
    val enabled: Boolean?,
    val icon: Bitmap?,
    val badgeText: String?,
    val badgeTextColor: Int?,
    val badgeBackgroundColor: Int?,
    val onClick: () -> Unit,
)
