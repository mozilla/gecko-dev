/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.interactor

import mozilla.components.ui.tabcounter.TabCounterMenu
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.toolbar.BrowserToolbarController
import org.mozilla.fenix.components.toolbar.BrowserToolbarMenuController
import org.mozilla.fenix.components.toolbar.ToolbarMenu

/**
 * Interface for the browser toolbar interactor. This interface is implemented by objects that
 * want to respond to user interaction on the browser toolbar.
 */
interface BrowserToolbarInteractor {
    fun onBrowserToolbarPaste(text: String)
    fun onBrowserToolbarPasteAndGo(text: String)
    fun onBrowserToolbarClicked()
    fun onBrowserToolbarMenuItemTapped(item: ToolbarMenu.Item)
    fun onTabCounterClicked()
    fun onTabCounterMenuItemTapped(item: TabCounterMenu.Item)
    fun onScrolled(offset: Int)
    fun onReaderModePressed(enabled: Boolean)

    /**
     * Navigates to the Home screen. Called when a user taps on the Home button.
     */
    fun onHomeButtonClicked()

    /**
     * Deletes all tabs and navigates to the Home screen. Called when a user taps on the erase button.
     */
    fun onEraseButtonClicked()

    /**
     * Opens the translation bottom sheet. Called when the user interacts with the translation
     * action.
     */
    fun onTranslationsButtonClicked()

    /**
     * Opens the share fragment.  Called when the user clicks the "Share" action in the toolbar.
     */
    fun onShareActionClicked()

    /**
     * Opens a new tab. Called when the user taps on the New Tab button.
     */
    fun onNewTabButtonClicked()

    /**
     * Called when the user long presses on the New Tab button.
     */
    fun onNewTabButtonLongClicked()

    /**
     * Opens the menu. Called when the user clicks the menu action button in the toolbar.
     *
     * @param accessPoint The [MenuAccessPoint] that was used to navigate to the menu dialog.
     * @param customTabSessionId The ID of the custom tab session if navigating from
     * an external access point, and null otherwise.
     * @param isSandboxCustomTab Whether or not the current custom tab is sandboxed.
     */
    fun onMenuButtonClicked(
        accessPoint: MenuAccessPoint,
        customTabSessionId: String? = null,
        isSandboxCustomTab: Boolean = false,
    )
}

/**
 * The default implementation of [BrowserToolbarInteractor].
 *
 * @param browserToolbarController [BrowserToolbarController] to which user actions can be
 * delegated for all interactions on the browser toolbar.
 * @param menuController [BrowserToolbarMenuController] to which user actions can be delegated
 * for all interactions on the the browser toolbar menu.
 */
class DefaultBrowserToolbarInteractor(
    private val browserToolbarController: BrowserToolbarController,
    private val menuController: BrowserToolbarMenuController,
) : BrowserToolbarInteractor {

    override fun onTabCounterClicked() {
        browserToolbarController.handleTabCounterClick()
    }

    override fun onTabCounterMenuItemTapped(item: TabCounterMenu.Item) {
        browserToolbarController.handleTabCounterItemInteraction(item)
    }

    override fun onBrowserToolbarPaste(text: String) {
        browserToolbarController.handleToolbarPaste(text)
    }

    override fun onBrowserToolbarPasteAndGo(text: String) {
        browserToolbarController.handleToolbarPasteAndGo(text)
    }

    override fun onBrowserToolbarClicked() {
        browserToolbarController.handleToolbarClick()
    }

    override fun onBrowserToolbarMenuItemTapped(item: ToolbarMenu.Item) {
        menuController.handleToolbarItemInteraction(item)
    }

    override fun onScrolled(offset: Int) {
        browserToolbarController.handleScroll(offset)
    }

    override fun onReaderModePressed(enabled: Boolean) {
        browserToolbarController.handleReaderModePressed(enabled)
    }

    override fun onHomeButtonClicked() {
        browserToolbarController.handleHomeButtonClick()
    }

    override fun onEraseButtonClicked() {
        browserToolbarController.handleEraseButtonClick()
    }

    override fun onTranslationsButtonClicked() {
        browserToolbarController.handleTranslationsButtonClick()
    }

    override fun onShareActionClicked() {
        browserToolbarController.onShareActionClicked()
    }

    override fun onNewTabButtonClicked() {
        browserToolbarController.handleNewTabButtonClick()
    }

    override fun onNewTabButtonLongClicked() {
        browserToolbarController.handleNewTabButtonLongClick()
    }

    override fun onMenuButtonClicked(
        accessPoint: MenuAccessPoint,
        customTabSessionId: String?,
        isSandboxCustomTab: Boolean,
    ) {
        browserToolbarController.handleMenuButtonClicked(accessPoint, customTabSessionId, isSandboxCustomTab)
    }
}
