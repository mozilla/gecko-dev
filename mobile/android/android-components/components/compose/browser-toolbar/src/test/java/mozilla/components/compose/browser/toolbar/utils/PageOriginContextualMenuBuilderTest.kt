/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.utils

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.compose.browser.toolbar.R
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.ContextualMenuOption
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.ContextualMenuOption.CopyURLToClipboard
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.ContextualMenuOption.PasteFromClipboard
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.CopyToClipboardClicked
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.LoadFromClipboardClicked
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.PasteFromClipboardClicked
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.utils.ClipboardHandler
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(sdk = [30]) // Run all tests on Android 11 because of better support for URL inference in tests
class PageOriginContextualMenuBuilderTest {
    private val clipboardUrl = "https://www.mozilla.org"
    private val clipboardText = "Mozilla"
    private val clipboard = ClipboardHandler(testContext)

    @Test
    fun `GIVEN all options are allowed and an URL exists in clipboard WHEN building the menu items THEN get all menu items`() {
        clipboard.text = clipboardUrl

        val result = PageOriginContextualMenuBuilder.buildMenuOptions(clipboard, ContextualMenuOption.entries)

        assertEquals(listOf(expectedCopyItem, expectedPasteItem, expectedLoadItem), result)
    }

    @Test
    fun `GIVEN all options are allowed and a regular text exists in clipboard WHEN building the menu items THEN don't get the item for loading an url`() {
        clipboard.text = clipboardText

        val result = PageOriginContextualMenuBuilder.buildMenuOptions(clipboard, ContextualMenuOption.entries)

        assertEquals(listOf(expectedCopyItem, expectedPasteItem), result)
    }

    @Test
    fun `GIVEN all options are allowed but no text exists in clipboard WHEN building the menu items THEN only get the menu item for copying the URL`() {
        val result = PageOriginContextualMenuBuilder.buildMenuOptions(clipboard, ContextualMenuOption.entries)

        assertEquals(listOf(expectedCopyItem), result)
    }

    @Test
    fun `GIVEN no options are allowed and an URL exists in clipboard WHEN building the menu items THEN don't get any menu items`() {
        clipboard.text = clipboardUrl

        val result = PageOriginContextualMenuBuilder.buildMenuOptions(clipboard, emptyList())

        assertEquals(emptyList<BrowserToolbarMenuButton>(), result)
    }

    @Test
    fun `GIVEN load is not allowed and an URL exists in clipboard WHEN building the menu items THEN don't get the item for loading an url`() {
        clipboard.text = clipboardText

        val result = PageOriginContextualMenuBuilder.buildMenuOptions(
            clipboard = clipboard,
            allowedMenuOptions = listOf(CopyURLToClipboard, PasteFromClipboard),
        )

        assertEquals(listOf(expectedCopyItem, expectedPasteItem), result)
    }

    @Test
    fun `GIVEN no paste is not allowed and an URL exists in clipboard WHEN building the menu items THEN only get the menu item for copying the URL`() {
        clipboard.text = clipboardUrl

        val result = PageOriginContextualMenuBuilder.buildMenuOptions(
            clipboard = clipboard,
            allowedMenuOptions = listOf(CopyURLToClipboard),
        )

        assertEquals(listOf(expectedCopyItem), result)
    }

    private companion object {
        val expectedCopyItem = BrowserToolbarMenuButton(
            iconResource = null,
            text = R.string.mozac_browser_toolbar_long_press_popup_copy,
            contentDescription = R.string.mozac_browser_toolbar_long_press_popup_copy,
            onClick = CopyToClipboardClicked,
        )
        val expectedPasteItem = BrowserToolbarMenuButton(
            iconResource = null,
            text = R.string.mozac_browser_toolbar_long_press_popup_paste,
            contentDescription = R.string.mozac_browser_toolbar_long_press_popup_paste,
            onClick = PasteFromClipboardClicked,
        )
        val expectedLoadItem = BrowserToolbarMenuButton(
            iconResource = null,
            text = R.string.mozac_browser_toolbar_long_press_popup_paste_and_go,
            contentDescription = R.string.mozac_browser_toolbar_long_press_popup_paste_and_go,
            onClick = LoadFromClipboardClicked,
        )
    }
}
