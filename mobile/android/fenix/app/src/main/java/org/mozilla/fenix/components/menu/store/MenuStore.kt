/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.store

import androidx.annotation.VisibleForTesting
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [MenuState] and applying [MenuAction]s.
 */
class MenuStore(
    initialState: MenuState,
    middleware: List<Middleware<MenuState, MenuAction>> = listOf(),
) : Store<MenuState, MenuAction>(
    initialState = initialState,
    reducer = ::reducer,
    middleware = middleware,
) {
    init {
        dispatch(MenuAction.InitAction)
    }
}

private fun reducer(state: MenuState, action: MenuAction): MenuState {
    return when (action) {
        is MenuAction.InitAction,
        is MenuAction.AddBookmark,
        is MenuAction.AddShortcut,
        is MenuAction.RemoveShortcut,
        is MenuAction.DeleteBrowsingDataAndQuit,
        is MenuAction.FindInPage,
        is MenuAction.OpenInApp,
        is MenuAction.OpenInFirefox,
        is MenuAction.InstallAddon,
        is MenuAction.CustomMenuItemAction,
        is MenuAction.ToggleReaderView,
        is MenuAction.CustomizeReaderView,
        is MenuAction.Navigate,
        is MenuAction.SaveMenuClicked,
        is MenuAction.ToolsMenuClicked,
        -> state

        is MenuAction.RequestDesktopSite -> state.copy(isDesktopMode = true)

        is MenuAction.RequestMobileSite -> state.copy(isDesktopMode = false)

        is MenuAction.UpdateExtensionState -> state.copyWithExtensionMenuState {
            it.copy(
                recommendedAddons = action.recommendedAddons,
            )
        }

        is MenuAction.UpdateWebExtensionBrowserMenuItems -> state.copyWithExtensionMenuState {
            it.copy(browserWebExtensionMenuItem = action.webExtensionBrowserMenuItem)
        }

        is MenuAction.UpdateWebExtensionPageMenuItems -> state.copyWithToolsMenuState {
            it.copy(pageWebExtensionMenuItem = action.webExtensionPageMenuItem)
        }

        is MenuAction.UpdateBookmarkState -> state.copyWithBrowserMenuState {
            it.copy(bookmarkState = action.bookmarkState)
        }

        is MenuAction.UpdatePinnedState -> state.copyWithBrowserMenuState {
            it.copy(isPinned = action.isPinned)
        }

        is MenuAction.UpdateInstallAddonInProgress -> state.copyWithExtensionMenuState {
            it.copy(addonInstallationInProgress = action.addon)
        }

        is MenuAction.InstallAddonFailed -> state.copyWithExtensionMenuState {
            it.copy(addonInstallationInProgress = null)
        }

        is MenuAction.InstallAddonSuccess -> state.copyWithExtensionMenuState { extensionState ->
            extensionState.copy(
                recommendedAddons = state.extensionMenuState.recommendedAddons.filter { it != action.addon },
                availableAddons = state.extensionMenuState.availableAddons.plus(action.addon),
                addonInstallationInProgress = null,
            )
        }

        is MenuAction.UpdateShowExtensionsOnboarding -> state.copyWithExtensionMenuState { extensionState ->
            extensionState.copy(showExtensionsOnboarding = action.showExtensionsOnboarding)
        }

        is MenuAction.UpdateShowDisabledExtensionsOnboarding -> state.copyWithExtensionMenuState { extensionState ->
            extensionState.copy(showDisabledExtensionsOnboarding = action.showDisabledExtensionsOnboarding)
        }

        is MenuAction.UpdateManageExtensionsMenuItemVisibility -> state.copyWithExtensionMenuState {
            it.copy(shouldShowManageExtensionsMenuItem = action.isVisible)
        }

        is MenuAction.UpdateAvailableAddons -> state.copyWithExtensionMenuState {
            it.copy(availableAddons = action.availableAddons)
        }
    }
}

@VisibleForTesting
internal inline fun MenuState.copyWithBrowserMenuState(
    crossinline update: (BrowserMenuState) -> BrowserMenuState,
): MenuState {
    return this.copy(browserMenuState = this.browserMenuState?.let { update(it) })
}

@VisibleForTesting
internal inline fun MenuState.copyWithExtensionMenuState(
    crossinline update: (ExtensionMenuState) -> ExtensionMenuState,
): MenuState {
    return this.copy(extensionMenuState = update(this.extensionMenuState))
}

@VisibleForTesting
internal inline fun MenuState.copyWithToolsMenuState(
    crossinline update: (ToolsMenuState) -> ToolsMenuState,
): MenuState {
    return this.copy(toolsMenuState = update(this.toolsMenuState))
}
