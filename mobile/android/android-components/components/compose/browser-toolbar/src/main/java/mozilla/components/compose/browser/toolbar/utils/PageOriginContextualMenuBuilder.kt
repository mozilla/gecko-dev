/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.utils

import androidx.compose.runtime.Stable
import mozilla.components.compose.browser.toolbar.R
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.ContextualMenuOption
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.ContextualMenuOption.CopyURLToClipboard
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.ContextualMenuOption.LoadFromClipboard
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.ContextualMenuOption.PasteFromClipboard
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton
import mozilla.components.support.utils.ClipboardHandler

/**
 * Build a list of [BrowserToolbarMenuButton] with options based on device's clipboard content
 * depending on what options are allowed.
 */
internal object PageOriginContextualMenuBuilder {

    /**
     * Build a list of [BrowserToolbarMenuButton] with options based on device's clipboard content
     * depending on what options are allowed.
     *
     * @param clipboard The [ClipboardHandler] to use for querying the device's clipboard.
     * @param allowedMenuOptions The list of [ContextualMenuOption] that should be shown if possible.
     */
    @Stable
    internal fun buildMenuOptions(
        clipboard: ClipboardHandler,
        allowedMenuOptions: List<ContextualMenuOption>,
    ): List<BrowserToolbarMenuButton> {
        if (allowedMenuOptions.isEmpty()) return emptyList()

        val containsText = clipboard.containsText()
        val containsUrl = clipboard.containsURL()

        return buildList {
            for (option in allowedMenuOptions) {
                when (option) {
                    CopyURLToClipboard -> {
                        add(
                            BrowserToolbarMenuButton(
                                iconResource = null,
                                text = R.string.mozac_browser_toolbar_long_press_popup_copy,
                                contentDescription = R.string.mozac_browser_toolbar_long_press_popup_copy,
                                onClick = option.event,
                            ),
                        )
                    }
                    PasteFromClipboard -> {
                        if (containsText) {
                            add(
                                BrowserToolbarMenuButton(
                                    iconResource = null,
                                    text = R.string.mozac_browser_toolbar_long_press_popup_paste,
                                    contentDescription = R.string.mozac_browser_toolbar_long_press_popup_paste,
                                    onClick = option.event,
                                ),
                            )
                        }
                    }
                    LoadFromClipboard -> {
                        if (containsUrl) {
                            add(
                                BrowserToolbarMenuButton(
                                    iconResource = null,
                                    text = R.string.mozac_browser_toolbar_long_press_popup_paste_and_go,
                                    contentDescription = R.string.mozac_browser_toolbar_long_press_popup_paste_and_go,
                                    onClick = option.event,
                                ),
                            )
                        }
                    }
                }
            }
        }
    }
}
