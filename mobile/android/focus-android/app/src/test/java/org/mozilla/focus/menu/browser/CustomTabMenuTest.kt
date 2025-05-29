/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu.browser

import android.content.Context
import android.content.res.Resources
import android.util.TypedValue
import mozilla.components.browser.menu.item.BrowserMenuCategory
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageSwitch
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.menu.item.WebExtensionPlaceholderMenuItem
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.any
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.eq
import org.mockito.Mockito.mock
import org.mockito.Mockito.`when`

class CustomTabMenuTest {

    private lateinit var context: Context
    private lateinit var mockTheme: Resources.Theme

    @Before
    fun setup() {
        context = mock()
        mockTheme = mock()

        `when`(context.getString(anyInt())).thenReturn("string")
        `when`(context.getString(anyInt(), anyString())).thenReturn("Powered by Focus")

        `when`(context.theme).thenReturn(mockTheme)

        `when`(mockTheme.resolveAttribute(anyInt(), any(), eq(true)))
            .thenAnswer { invocation ->
                val typedValueArg = invocation.arguments[1] as TypedValue
                typedValueArg.resourceId = 1
                true
            }
    }

    @Test
    fun `WHEN is onboarding tab is false THEN menu items contains all menu items`() {
        val customTabMenu = CustomTabMenu(
            context = context,
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
            context = context,
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
