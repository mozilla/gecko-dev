/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

import android.graphics.drawable.Drawable
import androidx.annotation.DrawableRes
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
    interface BrowserToolbarEvent : BrowserToolbarInteraction, BrowserToolbarAction {
        /**
         * Convenience method to combine dispatching a [BrowserToolbarEvent] with
         * showing a [BrowserToolbarMenu] for the same user interaction.
         *
         * @param menu [BrowserToolbarMenu] to show in addition to dispatching this event.
         */
        operator fun plus(menu: BrowserToolbarMenu) = CombinedEventAndMenu(
            event = this,
            menu = menu,
        )
    }

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

    /**
     * Combined [BrowserToolbarEvent] to be dispatched and [BrowserToolbarMenu] to be shown
     * for the same user interaction.
     *
     * @param event [BrowserToolbarEvent] to be dispatched when the menu is shown.
     * @param menu [BrowserToolbarMenu] to show.
     */
    data class CombinedEventAndMenu(
        val event: BrowserToolbarEvent,
        val menu: BrowserToolbarMenu,
    ) : BrowserToolbarInteraction
}

/**
 * Items which can be shown in a [BrowserToolbarMenu].
 */
sealed class BrowserToolbarMenuItem {
    /**
     * Button to shown in a [BrowserToolbarMenu].
     *
     * @property icon Optional [Drawable] icon for the menu item.
     * @property text The text shown for this item.
     * @property contentDescription Content description for this item.
     * @property onClick Optional [BrowserToolbarEvent] to be dispatched when this item is clicked.
     */
    data class BrowserToolbarMenuButton(
        val icon: Icon?,
        val text: Text,
        val contentDescription: ContentDescription,
        val onClick: BrowserToolbarEvent?,
    ) : BrowserToolbarMenuItem() {

        /**
         * The image to use as icon for this menu item.
         */
        sealed interface Icon {
            /**
             *  The [Drawable] as icon for this menu item.
             *
             *  @property drawable The [Drawable] to use as icon.
             *  @property shouldTint Whether or not to apply the application default tint to this icon.
             */
            data class DrawableIcon(
                val drawable: Drawable,
                val shouldTint: Boolean = true,
            ) : Icon

            /**
             * The [DrawableRes] as icon for this menu item.
             */
            @JvmInline
            value class DrawableResIcon(@DrawableRes val resourceId: Int) : Icon
        }

        /**
         * The text that this menu item should display.
         */
        sealed interface Text {
            /**
             * The [String] to display in this this menu item.
             */
            @JvmInline
            value class StringText(val text: String) : Text

            /**
             * The [StringRes] to display as text in this menu item.
             */
            @JvmInline
            value class StringResText(@StringRes val resourceId: Int) : Text
        }

        /**
         * The content description menu item.
         */
        sealed interface ContentDescription {
            /**
             * The [String] to use as content description of this menu item.
             */
            @JvmInline
            value class StringContentDescription(val text: String) : ContentDescription

            /**
             * The [StringRes] to use as content description of this menu item.
             */
            @JvmInline
            value class StringResContentDescription(@StringRes val resourceId: Int) : ContentDescription
        }
    }

    /**
     * Divider to show in a [BrowserToolbarMenu].
     */
    data object BrowserToolbarMenuDivider : BrowserToolbarMenuItem()
}
