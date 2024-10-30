/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.menu

import androidx.annotation.DrawableRes
import androidx.compose.material.DropdownMenuItem
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.text.Text

/**
 * Model for [DropdownMenuItem]. This is a sealed type to represent the different types of menu
 * items that can be rendered in the dropdown menu.
 */
sealed interface MenuItem {

    /**
     * [FixedItem]s are the ones that are pre-defined in the design system and have a fixed layout.
     * These are the ones that are rendered with a specific layout and have a specific behavior.
     *
     * @property text The text to be displayed in the menu item.
     * @property level The level of the menu item. This is used to determine the style of the menu.
     * @property onClick The action to be performed when the menu item is clicked.
     */
    sealed class FixedItem(
        open val text: Text,
        open val level: Level,
        open val onClick: () -> Unit,
    ) : MenuItem {

        /**
         * Type representing different levels of importance of a UI element.
         */
        enum class Level {
            /**
             * Default level of importance.
             */
            Default,

            /**
             * Critical level of importance.
             */
            Critical,
        }
    }

    /**
     * [TextItem] is a [FixedItem] that represents a menu item with only text.
     *
     * @property text [Text] to be displayed in the menu item.
     * @property level The level of the menu item. This is used to determine the style of the menu.
     * @property onClick The action to be performed when the menu item is clicked.
     */
    data class TextItem(
        override val text: Text,
        override val level: Level = Level.Default,
        override val onClick: () -> Unit,
    ) : FixedItem(
        text = text,
        level = level,
        onClick = onClick,
    )

    /**
     * [CheckableItem] is a [FixedItem] that represents a menu item with text and a check icon.
     *
     * @property text [Text] to be displayed in the menu item.
     * @property isChecked The state of the checkable item.
     * @property level The level of the menu item. This is used to determine the style of the menu.
     * @property onClick The action to be performed when the menu item is clicked.
     */
    data class CheckableItem(
        override val text: Text,
        val isChecked: Boolean,
        override val level: Level = Level.Default,
        override val onClick: () -> Unit,
    ) : FixedItem(
        text = text,
        level = level,
        onClick = onClick,
    )

    /**
     * [IconItem] is a [FixedItem] that represents a menu item with text and an icon.
     *
     * @property text [Text] to be displayed in the menu item.
     * @property drawableRes The drawable resource to be displayed in the menu item.
     * @property level The level of the menu item. This is used to determine the style of the menu.
     * @property onClick The action to be performed when the menu item is clicked.
     */
    data class IconItem(
        override val text: Text,
        @DrawableRes val drawableRes: Int,
        override val level: Level = Level.Default,
        override val onClick: () -> Unit,
    ) : FixedItem(
        text = text,
        level = level,
        onClick = onClick,
    )

    /**
     * [CustomMenuItem] can be used to render a custom content as a menu item. This should be used
     * sparingly and only for cases where the design system does not have a pre-defined layout for
     * the menu item.
     *
     * @property content The content to be displayed in the menu item.
     */
    data class CustomMenuItem(
        val content: @Composable () -> Unit,
    ) : MenuItem

    /**
     * [Divider] is a special item that represents a divider in the dropdown menu.
     */
    data object Divider : MenuItem
}
