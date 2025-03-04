/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu.browser

import mozilla.components.browser.menu.item.BrowserMenuCategory
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageSwitch
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.menu.item.WebExtensionPlaceholderMenuItem
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class CustomTabMenuTest {
    @Test
    fun `WHEN is onboarding tab is false THEN menu items contains all menu items`() {
        val customTabMenu = CustomTabMenu(
            context = testContext,
            store = BrowserStore(),
            currentTabId = "",
            isOnboardingTab = false,
        ) {}

        val expectedSize = 10
        val menuItems = customTabMenu.menuBuilder.items
        assertEquals(expectedSize, customTabMenu.menuBuilder.items.size)

        // Browser menu
        assertTrue(menuItems[0] is BrowserMenuItemToolbar)
        // Browser menu divider
        assertTrue(menuItems[1] is BrowserMenuDivider)
        // Find in page
        assertTrue(menuItems[2] is BrowserMenuImageText)
        // Desktop mode
        assertTrue(menuItems[3] is BrowserMenuImageSwitch)
        // Report site issue
        assertTrue(menuItems[4] is WebExtensionPlaceholderMenuItem)
        // Browser menu divider
        assertTrue(menuItems[5] is BrowserMenuDivider)
        // Add to homescreen
        assertTrue(menuItems[6] is BrowserMenuImageText)
        // Open in Focus
        assertTrue(menuItems[7] is SimpleBrowserMenuItem)
        // Open in...
        assertTrue(menuItems[8] is SimpleBrowserMenuItem)
        // Powered by
        assertTrue(menuItems[9] is BrowserMenuCategory)
    }

    @Test
    fun `WHEN is onboarding tab is true THEN menu items contains only sandboxed menu items`() {
        val customTabMenu = CustomTabMenu(
            context = testContext,
            store = BrowserStore(),
            currentTabId = "",
            isOnboardingTab = true,
        ) {}

        val expectedSize = 8
        val menuItems = customTabMenu.menuBuilder.items
        assertEquals(expectedSize, customTabMenu.menuBuilder.items.size)

        // Browser menu
        assertTrue(menuItems[0] is BrowserMenuItemToolbar)
        // Browser menu divider
        assertTrue(menuItems[1] is BrowserMenuDivider)
        // Find in page
        assertTrue(menuItems[2] is BrowserMenuImageText)
        // Desktop mode
        assertTrue(menuItems[3] is BrowserMenuImageSwitch)
        // Report site issue
        assertTrue(menuItems[4] is WebExtensionPlaceholderMenuItem)
        // Browser menu divider
        assertTrue(menuItems[5] is BrowserMenuDivider)
        // Add to homescreen
        assertTrue(menuItems[6] is BrowserMenuImageText)
        // Powered by
        assertTrue(menuItems[7] is BrowserMenuCategory)
    }
}
