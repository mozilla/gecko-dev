/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.tabstrip

import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.menu.MenuItem
import org.mozilla.fenix.compose.text.Text

/**
 * Model representing different tab strip tab counter menu items.
 */
sealed interface TabCounterMenuItem {

    /**
     * Model representing menu items with an icon.
     *
     * @property textResource The text resource to be displayed.
     * @property drawableRes The drawable resource to be displayed.
     * @property onClick Invoked when the item is clicked.
     */
    sealed class IconItem(
        @StringRes val textResource: Int,
        @DrawableRes val drawableRes: Int,
        open val onClick: () -> Unit,
    ) : TabCounterMenuItem {

        /**
         * Model representing a new tab menu item.
         *
         * @property onClick Invoked when the item is clicked.
         */
        data class NewTab(
            override val onClick: () -> Unit,
        ) : IconItem(
            textResource = R.string.add_tab,
            drawableRes = R.drawable.mozac_ic_plus_24,
            onClick = onClick,
        )

        /**
         * Model representing a new private tab menu item.
         *
         * @property onClick Invoked when the item is clicked.
         */
        data class NewPrivateTab(
            override val onClick: () -> Unit,
        ) : IconItem(
            textResource = R.string.add_private_tab,
            drawableRes = R.drawable.mozac_ic_private_mode_24,
            onClick = onClick,
        )

        /**
         * Model representing a close tab menu item.
         *
         * @property onClick Invoked when the item is clicked.
         */
        data class CloseTab(
            override val onClick: () -> Unit,
        ) : IconItem(
            textResource = R.string.close_tab,
            drawableRes = R.drawable.mozac_ic_cross_24,
            onClick = onClick,
        )
    }

    /**
     * Model representing a divider.
     */
    data object Divider : TabCounterMenuItem

    /**
     * Maps [TabCounterMenuItem] to a [MenuItem].
     */
    fun toMenuItem(): MenuItem =
        when (this) {
            is Divider -> MenuItem.Divider
            is IconItem -> MenuItem.IconItem(
                text = Text.Resource(textResource),
                drawableRes = drawableRes,
                onClick = onClick,
            )
        }
}
