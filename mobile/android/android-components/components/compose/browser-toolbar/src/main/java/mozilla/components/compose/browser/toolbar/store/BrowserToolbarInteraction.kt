/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

import android.graphics.drawable.Drawable
import androidx.annotation.StringRes
import androidx.compose.runtime.Immutable
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarMenu
import mozilla.components.lib.state.Action

/**
 * All possible ways to handle an interaction with browser toolbar elements.
 */
sealed interface BrowserToolbarInteraction {
    /**
     * [Action]s to be dispatched on [BrowserToolbarStore] when the user interacts with a toolbar element.
     */
    interface BrowserToolbarEvent : BrowserToolbarInteraction, BrowserToolbarAction

    /**
     * Popup menu to show when the user interacts with a toolbar element.
     */
    @Immutable
    fun interface BrowserToolbarMenu : BrowserToolbarInteraction {
        /**
         * List of items to show in the menu.
         */
        fun items(): List<BrowserToolbarMenuItem>
    }
}

/**
 * Item which can be shown in a [BrowserToolbarMenu].
 *
 * @property icon Optional icon to show in the popup item.
 * @property text Optional text to show in the popup item.
 * @property contentDescription Content description for this item. `null` if not important for accessibility.
 * @property onClick Optional [BrowserToolbarEvent] to be dispatched when this item is clicked.
 */
data class BrowserToolbarMenuItem(
    val icon: Drawable?,
    @StringRes val text: Int?,
    @StringRes val contentDescription: Int?,
    val onClick: BrowserToolbarEvent?,
)
